/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "fmr.h"
#include <cstring>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FM_HAL_BRIDGE"


#define RET_FALSE 0
#define RET_TRUE 1


static int g_idx = -1;
extern struct fmr_ds fmr_data;

bool openDev()
{
    int ret = 0;
    if(g_idx > -1){
      LOGD("%s, device has been opened g_idx=%d,just return",__func__,g_idx);
      return true;
    }
    if(g_idx = FMR_init() < 0) {
      LOGD("%s, [FMR_init =%d],open failed \n", __func__, g_idx);
      return false;
    }
    LOGD("%s, [g_idx=%d]\n", __func__, g_idx);
    ret = FMR_open_dev(g_idx); // if success, then ret = 0; else ret < 0

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}

bool closeDev()
{
    int ret = 0;

    ret = FMR_close_dev(g_idx);
    g_idx = -1; // should reset to null here
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}

bool powerUp(float freq)
{
    int ret = 0;
    int tmp_freq;

    LOGI("%s, [freq=%d]\n", __func__, (int)freq);
//    tmp_freq = (int)(freq * 10);        //Eg, 87.5 * 10 --> 875
    tmp_freq = (int)(freq/100);        //Eg,87500 -> 875
    ret = FMR_pwr_up(g_idx, tmp_freq);

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}
 /*
  * @param type (0, FMRadio; 1, FMTransimitter)
  * @return (true, success; false, failed)
 */
bool powerDown(int type)
{
    int ret = 0;

    ret = FMR_pwr_down(g_idx, type);

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}

int setStep(int step)
{
    int ret = 0;
    ret = FMR_set_step(g_idx, step);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

bool tune(float freq)
{
    int ret = 0;
    int tmp_freq;

//    tmp_freq = (int)(freq * 10);        //Eg, 87.5  --> 875
    tmp_freq = (int)(freq/10);        //Eg, 87500  --> 8750
    ret = FMR_tune(g_idx, tmp_freq);

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}

float seek(float freq, bool isUp, int spacing)
{
    int ret = 0;
    int tmp_freq;
    int ret_freq;
    float val;

    //tmp_freq = (int)(freq * 100);       //Eg, 87.55 * 100 --> 8755
    tmp_freq = (int)(freq/10);       //Eg, 87500 -->  8750
    spacing = spacing/10;

    ret = FMR_set_mute(g_idx, 1);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [mute] [ret=%d]\n", __func__, ret);

    ret = FMR_seek(g_idx, tmp_freq, (int)isUp, &ret_freq, spacing);
    if (ret) {
        ret_freq = tmp_freq; //seek error, so use original freq
    }

    LOGD("%s, [freq=%d] [ret=%d]\n", __func__, ret_freq, ret);

    //val = (float)ret_freq/100;   //Eg, 8755 / 100 --> 87.55
    val = (float)(ret_freq*10);   //Eg, 9210 --> 92100
    // mute should be unmuted when seek finish.
    ret = FMR_set_mute(g_idx, 0);
    if (ret) {
         LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [unmute] [ret=%d]\n", __func__, ret);
    return val;
}


int* autoScan(int* listNum, int spacing)
{

#define FM_SCAN_CH_SIZE_MAX 200
    int ret = 0;
    int* scanChlarray = NULL; // = new int[FM_SCAN_CH_SIZE_MAX];
    int chl_cnt = FM_SCAN_CH_SIZE_MAX;
    int ScanTBL[FM_SCAN_CH_SIZE_MAX];
    spacing = spacing/10;

    LOGI("%s, [tbl=%p]\n", __func__, ScanTBL);
    FMR_Pre_Search(g_idx);
    ret = FMR_scan(g_idx, ScanTBL, &chl_cnt, 0/*startFreq*/, spacing);
    if (ret < 0) {
        LOGE("scan failed!\n");
        scanChlarray = NULL;
        goto out;
    }
    if (chl_cnt > 0) {
        scanChlarray = new int[chl_cnt];
        memcpy(scanChlarray,ScanTBL,sizeof(int)*chl_cnt);
        for(int i=0;i<chl_cnt;i++){
          LOGD("after scan,memcpy,scanChlarray[%d]:%d",i,scanChlarray[i]);
        }
        *listNum = chl_cnt;
    } else {
        LOGE("cnt error, [cnt=%d]\n", chl_cnt);
        scanChlarray = NULL;
    }
    FMR_Restore_Search(g_idx);

    if (fmr_data.scan_stop == fm_true) {
        ret = FMR_tune(g_idx, fmr_data.cur_freq);
        LOGI("scan stop!!! tune ret=%d",ret);
    }

out:
    LOGD("%s, [cnt=%d] [ret=%d]\n", __func__, chl_cnt, ret);
    return scanChlarray;

}

short readRds()
{
    int ret = 0;
    uint16_t status = 0;

    ret = FMR_read_rds_data(g_idx, &status);

    if (ret) {
        LOGE("%s,status = 0,[ret=%d]\n", __func__, ret);
        status = 0; //there's no event or some error happened
    }

    return status;
}

/*
 * attention for result values
 * TODO - why ps also show incorrect data,with obsolete code?
 */
char* getPs()
{
    int ret = 0;
//    char* PSname = NULL;
    uint8_t *ps = NULL;
    int ps_len = 0;

    ret = FMR_get_ps(g_idx, &ps, &ps_len);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
        return NULL;
    }
//    PSname = new char[ps_len]; // where to delete?
//    memcpy(PSname,(const char*)ps, ps_len);
//    LOGD("%s, [ret=%d], [ps_len=%d],[ps=%s],[(const char*)ps=%s],[PSname=%s]\n", __func__, ret, ps_len, ps, (const char*)ps,PSname);
//  return PSname;
      LOGD("%s, [ret=%d], [ps_len=%d],[ps=%s],[(char*)ps=%s]\n", __func__, ret, ps_len, ps, (char*)ps);
    return (char*)ps;
}

int getBler()
{
    int ret = 0;
    int bler = -1;

    ret = FMR_get_bler(g_idx, &bler);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [bler=%d] [ret=%d]\n", __func__, bler, ret);
    return bler;
}

int getSnr()
{
    int ret = 0;
    int snr = -1;

    ret = FMR_get_snr(g_idx, &snr);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [snr=%d] [ret=%d]\n", __func__, snr, ret);
    return snr;
}

fm_seek_criteria_parm getTuneParm()
{
      int ret = 0;
      fm_seek_criteria_parm parm,p_error;
      memset(&parm, 0, sizeof(parm));
      memset(&p_error, 0, sizeof(p_error));

      ret = FMR_get_tune(g_idx, &parm);
      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
          return p_error;
      }

      LOGD("%s, [ret=%d]\n", __func__, ret);
      return parm;
}

int setTuneParm(fm_seek_criteria_parm para)
{
      int ret = 0;
      fm_seek_criteria_parm parm;
      memset(&parm, 0, sizeof(parm));

      memcpy(&parm,&para,8);
      LOGD("%s,set Tuneparm:%d,%d,%d,%d,%d",__func__,parm.rssi_th,parm.snr_th,parm.freq_offset_th,parm.pilot_power_th,parm.noise_power_th);
      ret = FMR_set_tune(g_idx, &parm);

      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
      }
      LOGD("%s, [ret=%d]\n", __func__, ret);

      return ret;
}

fm_audio_threshold_parm getAudioParm()
{
      int ret = 0;
      fm_audio_threshold_parm parm,p_error;
      memset(&p_error, 0, sizeof(p_error));
      memset(&parm, 0, sizeof(parm));

      ret = FMR_get_audio(g_idx, &parm);
      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
          return p_error;
      }

      LOGD("%s, [ret=%d]\n", __func__, ret);
      return parm;
}

int setAudioParm(fm_audio_threshold_parm para)
{
      int ret = 0;
      fm_audio_threshold_parm parm;
      memset(&parm, 0, sizeof(parm));

      memcpy(&parm,&para,sizeof(para));
      LOGD("%s,set Audioparm:%d,%d,%d,%d,%d",__func__,parm.hbound,parm.lbound,parm.power_th,parm.phyt,parm.snr_th);
      ret = FMR_set_audio(g_idx, &parm);

      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
      }
      LOGD("%s, [ret=%d]\n", __func__, ret);

      return ret;
}

fm_reg_ctl_parm getRegParm(unsigned int address)
{
      int ret = 0;
      fm_reg_ctl_parm fcp,p_error;
      memset(&fcp, 0, sizeof(fcp));
      memset(&p_error, 0, sizeof(p_error));

      fcp.err = 0;
      fcp.addr = address;
      fcp.rw_flag = 1;

      LOGD("%s,get reg address:%d,rw_flag:%d", __func__,fcp.addr,fcp.rw_flag);

      ret = FMR_rw_reg(g_idx, &fcp);
      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
          return p_error;
      }

      LOGD("%s, [ret=%d]\n", __func__, ret);
      return fcp;
}

int setRegParm(fm_reg_ctl_parm para)
{
      int ret = 0;
      fm_reg_ctl_parm parm;
      memset(&parm, 0, sizeof(parm));

      memcpy(&parm,&para,sizeof(para));
      LOGD("%s,set reg err:%d,address:%d,val:%d,rw_flag:%d", __func__,parm.err,parm.addr,parm.val,parm.rw_flag);
      ret = FMR_rw_reg(g_idx, &parm);

      if (ret) {
          LOGE("%s, error, [ret=%d]\n", __func__, ret);
      }
      LOGD("%s, [ret=%d]\n", __func__, ret);

      return ret;
}

//TODO - why LastRadioText also show incorrect data,with obsolete code?
char* getLrText()
{
    int ret = 0;
//    char* LastRadioText = NULL;
    uint8_t *rt = NULL;
    int rt_len = 0;

    ret = FMR_get_rt(g_idx, &rt, &rt_len);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
        return NULL;
    }
//    LastRadioText = new char[rt_len]; // where to delete?
//    memcpy(LastRadioText,(const char*)rt, rt_len);
//    LOGD("%s, [ret=%d],[rt_len=%d],[rt=%s],[(const char*)rt=%s],[LastRadioText=%s]\n", __func__, ret, rt_len, rt, (const char*)rt, LastRadioText);
//  return LastRadioText;
    LOGD("%s, [ret=%d],[rt_len=%d],[rt=%s],[(char*)rt=%s]\n", __func__, ret, rt_len, rt, (char*)rt);
    return (char*)rt;
}


short activeAf()
{
    int ret = 0;
    short ret_freq = 0;

    ret = FMR_active_af(g_idx, (uint16_t*)&ret_freq);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
        return 0;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret_freq;
}
// Todo,confirm shortArray, AFList,af.....
/*
jshortArray getAFList()
{
    int ret = 0;
    jshortArray AFList;
    char *af = NULL;
    int af_len = 0;

    //ret = FMR_get_af(g_idx, &af, &af_len); // If need, we should implemate this API
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
        return NULL;
    }
    AFList = env->NewShortArray(af_len);
    env->SetShortArrayRegion(AFList, 0, af_len, (const jshort*)af);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return AFList;
}
*/
int setRds(bool rdson)
{
    int ret = 0;
    int onoff = -1;

    if (rdson == RET_TRUE) {
        onoff = FMR_RDS_ON;
    } else {
        onoff = FMR_RDS_OFF;
    }
    ret = FMR_turn_on_off_rds(g_idx, onoff);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [onoff=%d] [ret=%d]\n", __func__, onoff, ret);
    return ret?RET_FALSE:RET_TRUE;
}

bool stopScan()
{
    int ret = 0;

    ret = FMR_stop_scan(g_idx);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?RET_FALSE:RET_TRUE;
}

int setMute(bool mute)
{
    int ret = 0;

    ret = FMR_set_mute(g_idx, (int)mute);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [mute=%d] [ret=%d]\n", __func__, (int)mute, ret);
    return ret?RET_FALSE:RET_TRUE;
}

/******************************************
 * Inquiry if RDS is support in driver.
 * Parameter:    None
 * Return Value:
 *      1: support
 *      0: NOT support
 *     -1: error
 ******************************************/
int isRdsSupport()
{
    int ret = 0;
    int supt = -1;

    ret = FMR_is_rdsrx_support(g_idx, &supt);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [supt=%d] [ret=%d]\n", __func__, supt, ret);
    return supt;
}

/******************************************
 * SwitchAntenna
 * Parameter: antenna:
                0 : switch to long antenna
                1: switch to short antenna
 *Return Value:
 *          0: Success
 *          1: Failed
 *          2: Not support
 ******************************************/
int switchAntenna(int antenna)
{
    int ret = 0;
    int jret = 0;
    int ana = -1;

    if (0 == antenna) {
        ana = FM_LONG_ANA;
    } else if (1 == antenna) {
        ana = FM_SHORT_ANA;
    } else {
        LOGE("%s:fail, para error\n", __func__);
        jret = RET_FALSE;
        goto out;
    }
    ret = FMR_ana_switch(g_idx, ana);
    if (ret == -ERR_UNSUPT_SHORTANA) {
        LOGW("Not support switchAntenna\n");
        jret = 2;
    } else if (ret) {
        LOGE("switchAntenna(), error\n");
        jret = 1;
    } else {
        jret = 0;
    }
out:
    LOGD("%s: [antenna=%d] [ret=%d]\n", __func__, ana, ret);
    return jret;
}

int getRssi()
{
    int ret = 0;
    int rssi = -1;

    ret = FMR_get_rssi(g_idx, &rssi);
    if (ret == -1) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [rssi=%d] [ret=%d]\n", __func__, rssi, ret);
    return rssi;
}

int SetRegion(int region)
{
    int ret = 0;
    int value = region;

    ret = FMR_set_region(g_idx, value);
    if (ret) {
        LOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    LOGD("%s, [region=%d] [ret=%d]\n", __func__, value, ret);
    return ret?RET_FALSE:RET_TRUE;
}


