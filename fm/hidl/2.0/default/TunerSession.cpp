/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "BcRadioDef.tuner"
#define LOG_NDEBUG 0

#include "TunerSession.h"
#include "BroadcastRadio.h"

#include <broadcastradio-utils-2x/Utils.h>
#include <log/log.h>
#include <pthread.h>
#include <android-base/strings.h>

#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <stdio.h>

namespace vendor {
namespace sprd {
namespace hardware {
namespace broadcastradio {
namespace V2_0 {
namespace implementation {


using namespace std::chrono_literals;
using namespace android::hardware::broadcastradio;
using namespace android::hardware::broadcastradio::utils;

using std::lock_guard;
using std::move;
using std::mutex;
using std::sort;
using std::vector;

namespace delay {

static constexpr auto seek = 0ms;
static constexpr auto step = 0ms;
static constexpr auto tune = 0ms;
//change to 0s for real scan
//static constexpr auto list = 0s;
static constexpr auto list = 0s;

}  // namespace delay

static int sprdrds_state = 0;
static int sprdtune_state = 0;
static int sprdselector_state = 0;
static int sprdscan_state = 0;

TunerSession::TunerSession(BroadcastRadio& module, const sp<ITunerCallback>& callback)
    : mIsTerminating(false), mCallback(callback), mModule(module) {
    bool result = openDev();
    ALOGD("TunerSession constructor...openDev :%d",result);
    if(result){
        mIsClosed = false;
        tuneInternalLocked(utils::make_selector_amfm(87500));

        if(1 == isRdsSupport()){
            mIsRdsSupported = true;
        }
        ALOGW("TunerSession constructor, mIsRdsSupported:%d",mIsRdsSupported);
        setRdsOnOff(true);
        if(mIsRdsSupported){
            mRdsUpdateThread = new std::thread(&TunerSession::rdsUpdateThreadLoop,this);
            sprdrds_state = 0;
            ALOGW("TunerSession:set sprdrds on");
        }
        ALOGW("TunerSession constructor, start rds thread ,powerup done...");
    }

}
/*
 * fm driver should powerdown when TunerSession destroy.
 */
TunerSession::~TunerSession(){
    ALOGD("~TunerSession powerdown...");
    //close();
    ALOGD("~TunerSession powerdown, before get mutex...");
    {
        lock_guard<mutex> lk(mRdsMut);
        mIsClosed = true;
        mIsTerminating = true;
        mCondRds.notify_one();
        ALOGD("~TunerSession powerdown, after set mIsTerminating true...");
    }
    if(nullptr != mRdsUpdateThread){
      mRdsUpdateThread->join();
      ALOGD("~TunerSession powerdown after join release done...");
    }
}
// makes ProgramInfo that points to no program
static ProgramInfo makeDummyProgramInfo(const ProgramSelector& selector) {
    ProgramInfo info = {};
    info.selector = selector;
    info.logicallyTunedTo = utils::make_identifier(
        IdentifierType::AMFM_FREQUENCY, utils::getId(selector, IdentifierType::AMFM_FREQUENCY));
    info.physicallyTunedTo = info.logicallyTunedTo;
    return info;
}
/*
* Add for rds update only,similar with makeDummyProgramInfo
* To make sure,do not update rds immediately when freq changed by seek.tune.scan... etc.
* Because fm driver need some time to fresh rds,when change to new program,so do not update rds in above function
*/
void TunerSession:: makeDummyProgramInfoForRdsUpdate(ProgramInfo* newInfo,const ProgramSelector& selector){
    newInfo->selector = selector;
    /*
     * Add for rds info, RDS events
     * PS :  RDS_EVENT_PROGRAMNAME = 0x0008;
     * RT :  RDS_EVENT_LAST_RADIOTEXT = 0x0040;
     */
      int rdsEvents = readRds();
      ALOGD("makeDummyProgramInfoForRdsUpdate,rds events : %d",rdsEvents);
      // when rds events = 0 ,there will be no rds info in program info ,so app will get null for ps and rt,update callback is unnecessary
      if(0 != rdsEvents){
        char* programName = "";
        char* rtName = "";
        if(rdsEvents & RDS_EVENT_PROGRAMNAME){
          programName = getPs();//get ps info
        }
        if(rdsEvents & RDS_EVENT_LAST_RADIOTEXT){
          rtName = getLrText();//get rt info
        }
        ALOGD("makeDummyProgramInfoForRdsUpdate,rds events : %d, ps:%s, rt:%s",rdsEvents,programName,rtName);

        newInfo->metadata = hidl_vec<Metadata>({
          make_metadata(MetadataKey::RDS_PS, programName),
          make_metadata(MetadataKey::RDS_RT, rtName),
          //make_metadata(MetadataKey::SONG_TITLE, songTitle),
          //make_metadata(MetadataKey::SONG_ARTIST, songArtist),
          //make_metadata(MetadataKey::STATION_ICON, resources::demoPngId),
          //make_metadata(MetadataKey::ALBUM_ART, resources::demoPngId),
        });// make metadata
      }else{
        newInfo->metadata = hidl_vec<Metadata>({
          // make metadata null
        });
      }
    newInfo->logicallyTunedTo = utils::make_identifier(
        IdentifierType::AMFM_FREQUENCY, utils::getId(selector, IdentifierType::AMFM_FREQUENCY));
    newInfo->physicallyTunedTo = newInfo->logicallyTunedTo;
}

/*
 * Add for read rds periodically in a single thread.
 * sleep(xx),how long will be correct?
 * start in constructor or not judged by mIsRdsSupported flag
*/
void TunerSession::rdsUpdateThreadLoop(){
  ALOGD("TunerSession, rdsUpdateThreadLoop  start");
  // rds supported,when fm powerup and no closed, update rds per x seconds.
  ProgramInfo newInfo = {};
  newInfo = makeDummyProgramInfo(mCurrentProgram);
  while(!mIsTerminating){
    std::unique_lock<mutex> lk(mRdsMut);
    ALOGD("TunerSession come into rdsUpdateThreadLoop ....mIsClosed :%d",mIsClosed);
    if(!mIsClosed){
      if(sprdrds_state == 0){
        makeDummyProgramInfoForRdsUpdate(&newInfo,mCurrentProgram);
      }
      if(isRdsUpdateNeeded(newInfo)){
        mCurrentProgramInfo = newInfo; // add for rds callback filter.update current programinfo
        auto task =[this,newInfo](){
          lock_guard<mutex> lk(mMut);
          mCallback->onCurrentProgramInfoChanged(newInfo);
        };
        mThread.schedule(task,delay::tune);
      }
      // wait for kTimeoutDuration,and break when timeout and mIsClosed = false;
      // but if timeout and mIsClosed = true, will not wait，run immediately.
      ALOGD("rdsUpdateThreadLoop, wait_for pred : %d",(true == mIsClosed));
      mCondRds.wait_for(lk,kTimeoutDuration,[&]{return mIsClosed;});// wait for update rds per 3s.
      ALOGD("TunerSession  rdsUpdateThreadLoop after wait_for sleep 3s....");
      lk.unlock();
   }else{
      // wait until open or ~TunerSession.
      ALOGD("rdsUpdateThreadLoop, wait pred : %d",(!mIsClosed || mIsTerminating));
      // wait when pred is false;  run when (mIsClosed = false or mIsTerminating = true)
      mCondRds.wait(lk,[&]{return !mIsClosed || mIsTerminating;});// wait for update rds per 3s.
      ALOGD("TunerSession  rdsUpdateThreadLoop after wait sleep 3s....");
      lk.unlock();
   }
  }
   ALOGD("TunerSession released, rds thread return....");
   // exit rdsthread function due to TunerSession
   return;
}

/*
*Add for judgement: whether should update rds info;
*@Result: true- update; false- do not update;
*/
bool TunerSession::isRdsUpdateNeeded(ProgramInfo& info){
    auto nps =  utils::getMetadataString(info, MetadataKey::RDS_PS);
    auto nrt =  utils::getMetadataString(info, MetadataKey::RDS_RT);
    ALOGD("rdsUpdateThreadLoop,newInfo.selector:%s, ps:%s, rt:%s",toString(info.selector).c_str(),nps->c_str(),nrt->c_str());
    //ALOGD("rdsUpdateThreadLoop,pss has_value:%d, rt has_value:%d",nps.has_value(),nrt.has_value());

    auto ops = utils::getMetadataString(mCurrentProgramInfo, MetadataKey::RDS_PS);
    auto ort = utils::getMetadataString(mCurrentProgramInfo, MetadataKey::RDS_RT);
    ALOGD("rdsUpdateThreadLoop,mCurrentProgramInfo.selector:%s, ps:%s, rt:%s",toString(mCurrentProgramInfo.selector).c_str(),ops->c_str(),ort->c_str());
    bool update = true;
    if((info.selector == mCurrentProgramInfo.selector)&&(android::base::Trim(*nps) == android::base::Trim(*ops))&&(android::base::Trim(*nrt) == android::base::Trim(*ort))){
       // freq、rds info are the same as last time, do not to update.
       update = false;
    }
    ALOGD("rdsUpdateThreadLoop, isRdsUpdateNeeded: %d", update);
    return update;
}

void TunerSession::tuneInternalLocked(const ProgramSelector& sel) {
    ALOGD("%s(%s)", __func__, toString(sel).c_str());

    VirtualProgram virtualProgram;
    ProgramInfo programInfo;
    int ret = 0;

    mCurrentProgram = sel;
    auto current = utils::getId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY);
    ALOGD("Tuner::tuneInternalLocked..tune.. current=%lu",current);
    if (mIsClosed) {
        sprdtune_state = 1;         //fix native crash
        ALOGD("%s set sprdtune_state before tune", __func__);
        return;
    }
    ::tune(current);
    programInfo = makeDummyProgramInfo(sel);
    mCurrentProgramInfo = programInfo; // add for rds callback filter.
    mIsTuneCompleted = true;
    setRdsOnOff(true);
    if (sprdscan_state == 1) {
        ret = setMute(0); //set unmute
        if (ret) {
           sprdscan_state = 0;
           ALOGD("set unmute after autoscan");
        }
    }
    mCallback->onCurrentProgramInfoChanged(programInfo);
}

const BroadcastRadio& TunerSession::module() const {
    return mModule.get();
}

const VirtualRadio& TunerSession::virtualRadio() const {
    return module().mVirtualRadio;
}

Return<Result> TunerSession::tune(const ProgramSelector& sel) {
    ALOGD("%s(%s)", __func__, toString(sel).c_str());
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;

    if (!utils::isSupported(module().mProperties, sel)) {
        ALOGW("Selector not supported");
        return Result::NOT_SUPPORTED;
    }

    if (!utils::isValid(sel)) {
        ALOGE("ProgramSelector is not valid");
        return Result::INVALID_ARGUMENTS;
    }

    cancelLocked();

    setRdsOnOff(false);
    mIsTuneCompleted = false;
    auto task = [this, sel]() {
        lock_guard<mutex> lk(mMut);
        tuneInternalLocked(sel);
    };
    mThread.schedule(task, delay::tune);

    return Result::OK;
}

Return<Result> TunerSession::scan(bool directionUp, bool /* skipSubChannel */) {
    ALOGD("%s", __func__);
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;
    cancelLocked();

 /*Original code here.
  * There maybe two path for this implement:
  *1- scan use seek(startfreq,isup);
  *2- autoscan fresh station list,and then seek this list
  *Attention: seek task timeout is 200ms default,so path 2 may be better.
  *need VTS test to verify.
  *remove original code here for real scan implements
  */
    setRdsOnOff(false);
    if (sprdselector_state == 1) {
        mCurrentProgram = utils::make_selector_amfm(87500);
        sprdselector_state = 0;
        ALOGD("set scan frequency: 87500");
    }
    auto current = utils::getId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY);
    float seekResult = seek(current,directionUp,mSpacing);
    ALOGD("scan station..,after seek,current=%lu, seekResult=%d",current,(int)seekResult);
    auto tuneTo = utils::make_selector_amfm((int)seekResult);

    mIsTuneCompleted = false;
    auto task = [this, tuneTo, directionUp]() {
        ALOGI("Performing seek up=%d", directionUp);

        lock_guard<mutex> lk(mMut);
        tuneInternalLocked(tuneTo);
    };
    mThread.schedule(task, delay::seek);

    return Result::OK;
}

Return<Result> TunerSession::step(bool directionUp) {
    ALOGD("%s", __func__);
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;

    cancelLocked();

    if (!utils::hasId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY)) {
        ALOGE("Can't step in anything else than AM/FM");
        return Result::NOT_SUPPORTED;
    }

    auto stepTo = utils::getId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY);
    auto range = getAmFmRangeLocked();
    if (!range) {
        ALOGE("Can't find current band");
        return Result::INTERNAL_ERROR;
    }

    setRdsOnOff(false);
    if (directionUp) {
        stepTo += mSpacing;
    } else {
        stepTo -= mSpacing;
    }
    if (stepTo > range->upperBound) stepTo = range->lowerBound;
    if (stepTo < range->lowerBound) stepTo = range->upperBound;

    mIsTuneCompleted = false;
    auto task = [this, stepTo]() {
        ALOGI("Performing step to %s", std::to_string(stepTo).c_str());

        lock_guard<mutex> lk(mMut);

        tuneInternalLocked(utils::make_selector_amfm(stepTo));
    };
    mThread.schedule(task, delay::step);

    return Result::OK;
}

void TunerSession::cancelLocked() {
    ALOGD("%s", __func__);

    mThread.cancelAll();
    if (utils::getType(mCurrentProgram.primaryId) != IdentifierType::INVALID) {
        mIsTuneCompleted = true;
    }
}

Return<void> TunerSession::cancel() {
    ALOGD("%s", __func__);
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return {};

    cancelLocked();

    return {};
}

Return<Result> TunerSession::startProgramListUpdates(const ProgramFilter& filter) {
    ALOGD("%s(%s)", __func__, toString(filter).c_str());
    lock_guard<mutex> lk(mMut);
    int ret = 0;
    if (mIsClosed) return Result::INVALID_STATE;
        setRdsOnOff(false);
        ret = setMute(1); //set mute
        if (ret) {
           sprdscan_state = 1;
           ALOGD("set mute before autoscan");
        }
       // real autoScan directly
        ALOGD("start autoScan..");
        int* results = nullptr;
        int length = 0;
        results = autoScan(&length, mSpacing);
        ALOGD("autoScan done..results: %p, length:%d",results,length);
        std::vector<VirtualProgram> mScanedPrograms;
        for(int i=0;i<length;i++){
           ALOGD("after autoScan..make program ,results[i] :%d",results[i]);
           mScanedPrograms.push_back({make_selector_amfm(10*results[i]),"","",""});
        }
        delete results; // need to be deleted after used
        setRdsOnOff(true);

        vector<VirtualProgram> filteredList;
        auto filterCb = [&filter](const VirtualProgram& program) {
            return utils::satisfies(filter, program.selector);
        };
        std::copy_if(mScanedPrograms.begin(), mScanedPrograms.end(), std::back_inserter(filteredList), filterCb);

        auto task = [this, mScanedPrograms]() {
            lock_guard<mutex> lk(mMut);

            ProgramListChunk chunk = {};
            chunk.purge = true;
            chunk.complete = true;
            chunk.modified = hidl_vec<ProgramInfo>(mScanedPrograms.begin(), mScanedPrograms.end());

            mCallback->onProgramListUpdated(chunk);
        };

    mThread.schedule(task, delay::list);

    return Result::OK;
}

/*Original code,no implement
 * Add stopScan for realization
 * SPRD: Don't implement stopScan after autoScan
 */
Return<void> TunerSession::stopProgramListUpdates() {
    ALOGD("%s", __func__);
    return {};
}

Return<void> TunerSession::isConfigFlagSet(ConfigFlag flag, isConfigFlagSet_cb _hidl_cb) {
    ALOGD("%s(%s)", __func__, toString(flag).c_str());

    _hidl_cb(Result::NOT_SUPPORTED, false);
    return {};
}

Return<Result> TunerSession::setConfigFlag(ConfigFlag flag, bool value) {
    ALOGD("%s(%s, %d)", __func__, toString(flag).c_str(), value);

    return Result::NOT_SUPPORTED;
}

Return<void> TunerSession::setParameters(const hidl_vec<VendorKeyValue>& parameters,
                                         setParameters_cb _hidl_cb) {
    ALOGD("%s parameters length = %d", __func__,parameters.size());
    lock_guard<mutex> lk(mSetParametersMut);
    vector<VendorKeyValue> result;
    for(size_t i = 0; i < parameters.size(); ++i) {
        if("spacing" == parameters[i].key){
            ALOGD("set spacing %s",parameters[i].value.c_str());
            int ret = 0;
            if ("50k" == parameters[i].value) {
                mSpacing = 50;
                ret = setStep(0); //SCAN_STEP_50KHZ 0
            } else if("100k" == parameters[i].value) {
                mSpacing = 100;
                ret = setStep(1); //SCAN_STEP_100KHZ 1
            }
            hidl_vec<VendorKeyValue> vec = {{"spacing",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
        }else if("antenna" == parameters[i].key){
            int ret = 0;
            ALOGD("switch antenna %s",parameters[i].value.c_str());
            ret = switchAntenna(parameters[i].value == "0" ? 0 : 1);
            hidl_vec<VendorKeyValue> vec = {{"antenna",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
        }else if("stopScan" == parameters[i].key){
            int ret = 0;
            ALOGD("stopScan %s",parameters[i].value.c_str());
            ret = stopScan() ? 1 : 0;
            hidl_vec<VendorKeyValue> vec = {{"stopScan",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
        }else if("sprdsetrds" == parameters[i].key){
            ALOGD("set sprdsetrds %s",parameters[i].value.c_str());
            int ret = 0;
            if ("sprdrdson" == parameters[i].value) {
                ret = setRds(1); //set rds on
                sprdrds_state = 0;
                ALOGD("set sprdrds on");
            } else if("sprdrdsoff" == parameters[i].value) {
                ret = setRds(0); //set rds off
                sprdrds_state = 1;
                ALOGD("set sprdrds off");
                }
            hidl_vec<VendorKeyValue> vec = {{"sprdsetrds",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
       }else if("selector" == parameters[i].key){
            ALOGD("set selector %s",parameters[i].value.c_str());
            int ret = 0;
            if ("1" == parameters[i].value) {
                sprdselector_state = 1;
                ALOGD("selector state: on");
            }
            hidl_vec<VendorKeyValue> vec = {{"selector",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
       }else if("region" == parameters[i].key){
            ALOGD("set selector %s",parameters[i].value.c_str());
            int ret = 0;
            if ("0" == parameters[i].value) {
                ret = SetRegion(0);
                ALOGD("set region: 0");
            }else if ("1" == parameters[i].value) {
                ret = SetRegion(1);
                ALOGD("set region: 1");
            }else if ("2" == parameters[i].value) {
                ret = SetRegion(2);
                ALOGD("set region: 2");
            }else if ("3" == parameters[i].value) {
                ret = SetRegion(3);  //test 7600-10800
                ALOGD("set region: 3");
            }
            hidl_vec<VendorKeyValue> vec = {{"region",std::to_string(ret)}};
            _hidl_cb(vec);
            return Void();
       }else if("settuneparm" == parameters[i].key){
            ALOGD("set engineer tuneparm %s",parameters[i].value.c_str());
            char tuneparm[30];
            int ret_set = 0;
            fm_seek_criteria_parm set_para;

            char temp_rssi_th[4] = {0};
            char temp_snr_th[4] = {0};
            char temp_freq_offset_th[6] = {0};
            char temp_pilot_power_th[6] = {0};
            char temp_noise_power_th[6] = {0};
            int count = 0;
            int count_temp = 0;
            int count_node = 0;
            int j = 0;

            int length = strlen(parameters[i].value.c_str());
            if (length >= 30) {
                ALOGD("tune parameters length is = %d,should < 30!",length);
                ret_set = -1;
                hidl_vec<VendorKeyValue> vec = {{"settuneparm",std::to_string(ret_set)}};
                _hidl_cb(vec);
                return Void();
            }
            memcpy(tuneparm,parameters[i].value.c_str(),length);
            ALOGD("seting tuneparm key and value");

            for (j = 0;j < length; j++) {
                if (tuneparm[j] != '.') {
                    count ++;
                } else if (tuneparm[j] == '.'|| tuneparm[j] == '\0'){
                    count_node ++;
                    if (count_node == 1) {
                        memcpy(temp_rssi_th,&tuneparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 2) {
                        memcpy(temp_snr_th,&tuneparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 3) {
                        memcpy(temp_freq_offset_th,&tuneparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 4) {
                        memcpy(temp_pilot_power_th,&tuneparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                        count_node ++;
                    }
                }
                if (count_node == 5) {
                    memcpy(temp_noise_power_th,&tuneparm[count_temp+count_node-1],count- count_temp);
                }
            }

            set_para.rssi_th =(unsigned char)atoi(temp_rssi_th);
            set_para.snr_th =(unsigned char)atoi(temp_snr_th);
            set_para.freq_offset_th = (unsigned short)atoi(temp_freq_offset_th);
            set_para.pilot_power_th = (unsigned short)atoi(temp_pilot_power_th);
            set_para.noise_power_th = (unsigned short)atoi(temp_noise_power_th);
            ALOGD("set TuneParm:%d,%d,%d,%d,%d",set_para.rssi_th,set_para.snr_th,set_para.freq_offset_th,set_para.pilot_power_th,set_para.noise_power_th);

            ret_set = setTuneParm(set_para);
            ALOGD("action setTuneParm:%d",ret_set);

            hidl_vec<VendorKeyValue> vec = {{"settuneparm",std::to_string(ret_set)}};
            _hidl_cb(vec);
            return Void();
        }else if("setaudioparm" == parameters[i].key){
            ALOGD("set engineer audioparm: %s",parameters[i].value.c_str());
            char audioparm[30];
            int ret_set = 0;
            fm_audio_threshold_parm  set_para;
            char temp_hbound[6] = {0};
            char temp_lbound[6] = {0};
            char temp_power_th[6] = {0};
            char temp_phyt[4] = {0};
            char temp_snr_th[4] = {0};
            int count = 0;
            int count_temp = 0;
            int count_node = 0;
            int j = 0;

            int length = strlen(parameters[i].value.c_str());
             if (length >= 30) {
                ALOGD("audio parameters length is = %d,should < 30!",length);
                ret_set = -1;
                hidl_vec<VendorKeyValue> vec = {{"settuneparm",std::to_string(ret_set)}};
                _hidl_cb(vec);
                return Void();
            }
            memcpy(audioparm,parameters[i].value.c_str(),length);
            ALOGD("seting audioparm key and value");

            for (j = 0;j < length; j++) {
                if (audioparm[j] != '.') {
                    count ++;
                } else if (audioparm[j] == '.'|| audioparm[j] == '\0'){
                    count_node ++;
                    if (count_node == 1) {
                        memcpy(temp_hbound,&audioparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 2) {
                        memcpy(temp_lbound,&audioparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 3) {
                        memcpy(temp_power_th,&audioparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 4) {
                        memcpy(temp_phyt,&audioparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                        count_node ++;
                    }
                }
                if (count_node == 5) {
                    memcpy(temp_snr_th,&audioparm[count_temp+count_node-1],count- count_temp);
                }
            }
            set_para.hbound =(unsigned short)atoi(temp_hbound);
            set_para.lbound=(unsigned short)atoi(temp_lbound);
            set_para.power_th = (unsigned short)atoi(temp_power_th);
            set_para.phyt = (unsigned char)atoi(temp_phyt);
            set_para.snr_th= (unsigned char)atoi(temp_snr_th);
            ALOGD("set AudioParm:%d,%d,%d,%d,%d",set_para.hbound,set_para.lbound,set_para.power_th,set_para.phyt,set_para.snr_th);

            ret_set = setAudioParm(set_para);
            ALOGD("action setAudioParm:%d",ret_set);

            hidl_vec<VendorKeyValue> vec = {{"setaudioparm",std::to_string(ret_set)}};
            _hidl_cb(vec);
            return Void();
       }else if("setregparm" == parameters[i].key){
            ALOGD("set engineer regparm %s",parameters[i].value.c_str());
            char regparm[40];
            int ret_set = 0;
            fm_reg_ctl_parm set_para;
            char temp_err[4] = {0};
            char temp_addr[15] = {0};
            char temp_val[15] = {0};
            char temp_rw_flag[4] = {0};
            int count = 0;
            int count_temp = 0;
            int count_node = 0;
            int j = 0;

            int length = strlen(parameters[i].value.c_str());
            if (length >= 40) {
                ALOGD("setreg parameters length is = %d too long,should < 40!",length);
                ret_set = -1;
                hidl_vec<VendorKeyValue> vec = {{"settuneparm",std::to_string(ret_set)}};
                _hidl_cb(vec);
                return Void();
            }
            memcpy(regparm,parameters[i].value.c_str(),length);
            ALOGD("seting regparm key and value");

            for (j = 0;j < length; j++) {
                if (regparm[j] != '.') {
                   count ++;
                } else if (regparm[j] == '.'|| regparm[j] == '\0'){
                    count_node ++;
                    if (count_node == 1) {
                        memcpy(temp_err,&regparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 2) {
                        memcpy(temp_addr,&regparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                    } else if (count_node == 3) {
                        memcpy(temp_val,&regparm[count_temp+count_node-1],count- count_temp);
                        count_temp = count;
                        count_node ++;
                    }
                }
                if (count_node == 4) {
                    memcpy(temp_rw_flag,&regparm[count_temp+ count_node -1],count- count_temp);
                }
            }
            set_para.err =(unsigned char)atoi(temp_err);
            set_para.addr =(unsigned int)atoi(temp_addr);
            set_para.val = (unsigned int)atoi(temp_val);
            set_para.rw_flag = (unsigned char)atoi(temp_rw_flag);
            ALOGD("set RegParm:%d,%d,%d,%d",set_para.err,set_para.addr,set_para.val,set_para.rw_flag);

            ret_set = setRegParm(set_para);
            ALOGD("action setRegParm:%d",ret_set);

            hidl_vec<VendorKeyValue> vec = {{"setregparm",std::to_string(ret_set)}};
            _hidl_cb(vec);
            return Void();
       }
    }
    _hidl_cb({});
    return {};
}

Return<void> TunerSession::getParameters(const hidl_vec<hidl_string>&  keys,
                                         getParameters_cb _hidl_cb) {
    ALOGD("%s keys length = %d", __func__,keys.size());
    char * reg = "getregparm";
    char mark[1] = {'.'};
    for(size_t i = 0; i < keys.size(); ++i) {
        int length = strlen(keys[i].c_str());
        if(keys[i] == "rssi") {
            int rssi = getRssi();
            hidl_vec<VendorKeyValue> vec = {{"rssi",std::to_string(rssi)}};
            _hidl_cb(vec);
            return Void();
        }else if(keys[i] == "bler") {
            int bler = getBler();
            hidl_vec<VendorKeyValue> vec = {{"bler",std::to_string(bler)}};
            _hidl_cb(vec);
            return Void();
        } else if(keys[i] == "snr") {
            int snr = getSnr();
            hidl_vec<VendorKeyValue> vec = {{"snr",std::to_string(snr)}};
            _hidl_cb(vec);
            return Void();
        } else if(keys[i] == "gettuneparm"){
            int ret_get = 0;
            char output[30] = {0};
            char temp_rssi_th[4],temp_snr_th[4],temp_freq_offset_th[6];
            char temp_pilot_power_th[6];
            char temp_noise_power_th[6];

            fm_seek_criteria_parm get_para;
            memset(&get_para, 0, sizeof(get_para));
            ALOGD("get engineer tuneparm");
            get_para = getTuneParm();
            ALOGD("get TuneParm:%d,%d,%d,%d,%d",get_para.rssi_th,get_para.snr_th,get_para.freq_offset_th,get_para.pilot_power_th,get_para.noise_power_th);

            sprintf(temp_rssi_th,"%d",get_para.rssi_th);
            strcat(output,temp_rssi_th);
            strcat(output,mark);
            sprintf(temp_snr_th,"%d",get_para.snr_th);
            strcat(output,temp_snr_th);
            strcat(output,mark);
            sprintf(temp_freq_offset_th,"%d",get_para.freq_offset_th);
            strcat(output,temp_freq_offset_th);
            strcat(output,mark);
            sprintf(temp_pilot_power_th,"%d",get_para.pilot_power_th);
            strcat(output,temp_pilot_power_th);
            strcat(output,mark);
            sprintf(temp_noise_power_th,"%d",get_para.noise_power_th);
            strcat(output,temp_noise_power_th);
            ALOGD("get output tuneParm:%s",output);

            hidl_vec<VendorKeyValue> vec = {{"gettuneparm",output}};
            _hidl_cb(vec);
            return Void();
       } else if(keys[i] == "getaudioparm"){
            int ret_get = 0;
            char output[30] = {0};
            char temp_hbound[6],temp_lbound[6],temp_power_th[6];
            char temp_phyt[4];
            char temp_snr_th[4];
            fm_audio_threshold_parm get_para;
            memset(&get_para, 0, sizeof(get_para));
            ALOGD("get engineer audioparm");
            get_para = getAudioParm();
            ALOGD("get AudioParm:%d,%d,%d,%d,%d",get_para.hbound,get_para.lbound,get_para.power_th,get_para.phyt,get_para.snr_th);

            sprintf(temp_hbound,"%d",get_para.hbound);
            strcat(output,temp_hbound);
            strcat(output,mark);
            sprintf(temp_lbound,"%d",get_para.lbound);
            strcat(output,temp_lbound);
            strcat(output,mark);
            sprintf(temp_power_th,"%d",get_para.power_th);
            strcat(output,temp_power_th);
            strcat(output,mark);
            sprintf(temp_phyt,"%d",get_para.phyt);
            strcat(output,temp_phyt);
            strcat(output,mark);
            sprintf(temp_snr_th,"%d",get_para.snr_th);
            strcat(output,temp_snr_th);

            ALOGD("get output audioParm:%s",output);

            hidl_vec<VendorKeyValue> vec = {{"getaudioparm",output}};
            _hidl_cb(vec);
            return Void();
       }else if((length >= 10 && length <= 30) && memcmp(reg,keys[i].c_str(),10) == 0){
            char regparm[30];
            char temp_regparm[20];
            char temp_err[4],temp_addr[15];
            char temp_val[15],temp_rw_flag[4];
            unsigned int addr = 0;
            int ret_get = 0;
            char output[40] = {0};
            fm_reg_ctl_parm get_para;
            memset(&get_para, 0, sizeof(get_para));

            ALOGD("get engineer regparm length:%d",length);
            memcpy(regparm,keys[i].c_str(),length);
            memcpy(temp_regparm,&regparm[10],length-10);

            addr =(unsigned int)atoi(temp_regparm);
            ALOGD("get RegParm addr:%d",addr);
            get_para = getRegParm(addr);
            ALOGD("get RegParm:%d,%d,%d,%d",get_para.err,get_para.addr,get_para.val,get_para.rw_flag);

            sprintf(temp_err,"%d",get_para.err);
            strcat(output,temp_err);
            strcat(output,mark);
            sprintf(temp_addr,"%d",get_para.addr);
            strcat(output,temp_addr);
            strcat(output,mark);
            sprintf(temp_val,"%d",get_para.val);
            strcat(output,temp_val);
            strcat(output,mark);
            sprintf(temp_rw_flag,"%d",get_para.rw_flag);
            strcat(output,temp_rw_flag);
            ALOGD("get output regParm:%s",output);

            hidl_vec<VendorKeyValue> vec = {{"getregparm",output}};
            _hidl_cb(vec);
            return Void();
       }
    }
    _hidl_cb({});
    return {};
}

Return<void> TunerSession::close() {
    ALOGD("%s", __func__);
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return {};
    {
      ALOGD("TunerSession close before set mIsClosed true...");
      lock_guard<mutex> lk(mRdsMut);
      mIsClosed = true;
      setRdsOnOff(false);
      closeDev();
      mCondRds.notify_one();
      ALOGD("TunerSession close after set mIsClosed true...");
    }
    mThread.cancelAll();

    return {};
}

std::optional<AmFmBandRange> TunerSession::getAmFmRangeLocked() const {
    if (!mIsTuneCompleted) {
        ALOGW("tune operation in process");
        return {};
    }
    if (!utils::hasId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY)) return {};

    auto freq = utils::getId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY);
    for (auto&& range : module().getAmFmConfig().ranges) {
        if (range.lowerBound <= freq && range.upperBound >= freq) return range;
    }

    return {};
}

// set rds function on/off, remove to BroadcastRadio
bool TunerSession::setRdsOnOff(bool rdsOn){
    ALOGD("setRdsOnOff, set rds :%d", rdsOn);
    int result = 0;
    if(mIsRdsSupported){
      result = setRds(rdsOn);
    }else{
      ALOGD("setRdsOnOff, rds not support");
      return false;
    }

    return result ? true:false;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace sprd
}  // namespace vendor
