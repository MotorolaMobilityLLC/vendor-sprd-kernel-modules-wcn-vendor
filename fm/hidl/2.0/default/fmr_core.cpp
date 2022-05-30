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

/*******************************************************************
 * FM JNI core
 * return -1 if error occured. else return needed value.
 * if return type is char *, return NULL if error occured.
 * Do NOT return value access paramater.
 *
 * FM JNI core should be independent from lower layer, that means
 * there should be no low layer dependent data struct in FM JNI core
 *
 * Naming rule: FMR_n(paramter Micro), FMR_v(functions), fmr_n(global param)
 * pfmr_n(global paramter pointer)
 *
 *******************************************************************/

#include "fmr.h"
#include <stdio.h>
#include <stdlib.h>  
#include <stdarg.h>
#include <unistd.h>  
#include <cutils/sockets.h>
#include <signal.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FMHAL_CORE"
#define FMR_CONFIG_FILE "/vendor/etc/fm.conf"

#define FMR_MAX_IDX 1
#define FMR_MAX_FAKE_CHN_NUM 50

struct fmr_ds fmr_data;
struct fmr_ds *pfmr_data[FMR_MAX_IDX] = {0};
struct fm_fake_channel gFmFakeChnList[FMR_MAX_FAKE_CHN_NUM];
struct fm_fake_channel_t gFakeChn;

#define FMR_fd(idx) ((pfmr_data[idx])->fd)
#define FMR_err(idx) ((pfmr_data[idx])->err)

#define FMR_chip(idx) ((pfmr_data[idx])->cfg_data.chip)
#define FMR_band(idx) ((pfmr_data[idx])->cfg_data.band)
#define FMR_low_band(idx) ((pfmr_data[idx])->cfg_data.low_band)
#define FMR_high_band(idx) ((pfmr_data[idx])->cfg_data.high_band)
#define FMR_seek_space(idx) ((pfmr_data[idx])->cfg_data.seek_space)
#define FMR_max_scan_num(idx) ((pfmr_data[idx])->cfg_data.max_scan_num)
#define FMR_seek_lev(idx) ((pfmr_data[idx])->cfg_data.seek_lev)
#define FMR_scan_sort(idx) ((pfmr_data[idx])->cfg_data.scan_sort)
#define FMR_short_ana_sup(idx) ((pfmr_data[idx])->cfg_data.short_ana_sup)
#define FMR_rssi_th_l2(idx) ((pfmr_data[idx])->cfg_data.rssi_th_l2)
#define FMR_fake_chan(idx) ((pfmr_data[idx])->cfg_data.fake_chan)

#define FMR_cbk_tbl(idx) ((pfmr_data[idx])->tbl)
#define FMR_cust_hdler(idx) ((pfmr_data[idx])->custom_handler)
#define FMR_get_cfg(idx) ((pfmr_data[idx])->get_cfg)

static void killer(int sig) ;
	
int FMR_get_cfgs(int idx)
{   
    FILE *fp;
    int mFakeCounter = 0;
    char curLine[1024];  
    char *ret;
    char *valueStr;  

    memset(gFmFakeChnList, 0, sizeof (struct fm_fake_channel) * FMR_MAX_FAKE_CHN_NUM);
    FMR_fake_chan(idx) = &gFakeChn;
    gFakeChn.chan = gFmFakeChnList;
    LOGE("open file:%s \n", FMR_CONFIG_FILE);
    if((fp = fopen(FMR_CONFIG_FILE, "r")) == NULL)
    {   
        LOGE("open file:%s fail\n", FMR_CONFIG_FILE);
        return 0;
    }
    while(!feof(fp))
    {
        curLine[0] = '\0';
        ret = fgets(curLine, 1024, fp);
        if(ret == NULL) {
            break;
        }
        LOGD("get line:%s \n", curLine);

        /* remove the comments */
        valueStr= strchr(curLine, '#');
        if (NULL != valueStr){
            valueStr[0] = '\0';
        } 

        valueStr= strchr(curLine, '=');
        if (NULL != valueStr){
            valueStr[0] = '\0';
            valueStr = &valueStr[1];
        }
        else  {
             continue;
         }

        while (valueStr[0] == ' ' || valueStr[0] == '\t') valueStr++;
        while (valueStr[strlen(valueStr) - 1] == ' ' || valueStr[strlen(valueStr) - 1] == '\t') valueStr[strlen(valueStr) - 1] = '\0';
        while (curLine[strlen(curLine) - 1] == ' ' || curLine[strlen(curLine) - 1] == '\t') curLine[strlen(curLine) - 1] = '\0';

        LOGD("title:%s,  value:%s\n", curLine, valueStr);

        if (!strcmp(curLine, "chip"))  FMR_chip(idx) = atoi(valueStr);
        if (!strcmp(curLine, "band"))  FMR_band(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "low band"))  FMR_low_band(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "high band"))   FMR_high_band(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "seek space"))  FMR_seek_space(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "max scan num"))  FMR_max_scan_num(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "seek level"))  FMR_seek_lev(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "scan sort"))  FMR_scan_sort(idx) = atoi(valueStr);
        if (!strcmp(curLine, "short antenna support"))  FMR_short_ana_sup(idx)  = atoi(valueStr);
        if (!strcmp(curLine, "rssi threshold"))  FMR_rssi_th_l2(idx)  = atoi(valueStr);

        if (!strcmp(curLine, "fake channel")) {
            sscanf(valueStr, "%d;%d;%d", &gFmFakeChnList[mFakeCounter].freq, &gFmFakeChnList[mFakeCounter].rssi_th, &gFmFakeChnList[mFakeCounter].reserve);
            mFakeCounter++;
        }

    }
    gFakeChn.size = mFakeCounter;

    LOGD("chip: %d, band: %d, low_band: %d, high_band: %d, seek_space: %d, max_scan_num: %d, seek_lev: %d, scan_sort: %d, short_ana_sup: %d, rssi_th_l2: %d, mFakeCounter:%d ",  \
		pfmr_data[idx]->cfg_data.chip,  \
		pfmr_data[idx]->cfg_data.band,  \
		pfmr_data[idx]->cfg_data.low_band, \
		pfmr_data[idx]->cfg_data.high_band, \
		pfmr_data[idx]->cfg_data.seek_space, \
		pfmr_data[idx]->cfg_data.max_scan_num, \
		pfmr_data[idx]->cfg_data.seek_lev, \
		pfmr_data[idx]->cfg_data.scan_sort, \
		pfmr_data[idx]->cfg_data.short_ana_sup, \
		pfmr_data[idx]->cfg_data.rssi_th_l2, \
		mFakeCounter);

    fclose(fp);
    return 1;
}

int FMR_init()
{
    int idx = 0;
    int ret = 0;
    //signal(4, sig_alarm);
    if (pfmr_data[idx] != NULL) {
        pfmr_data[idx] = NULL;
    }
    LOGI("FMR idx = %d\n", idx);
    if (idx == FMR_MAX_IDX) {
        //FMR_seterr(ERR_NO_MORE_IDX);
        return -1;
    }

    /*The best way here is to call malloc to alloc mem for each idx,but
    I do not know where to release it, so use static global param instead*/
    pfmr_data[idx] = &fmr_data;
    memset(pfmr_data[idx], 0, sizeof(struct fmr_ds));

    if (FMR_get_cfgs(idx) < 0) {
        LOGI("FMR_get_cfgs failed\n");
        goto fail;
    }

    pfmr_data[idx]->init_func = FM_interface_init;
    if (pfmr_data[idx]->init_func == NULL) {
        LOGE("%s init_func error, %s\n", __func__, dlerror());
        goto fail;
    } else {
        LOGI("Go to run init function\n");
        (*pfmr_data[idx]->init_func)(&(pfmr_data[idx]->tbl));
        LOGI("OK\n");
        ret = 0;
    }

    signal(SIGINT, killer);

    return idx;

fail:
    pfmr_data[idx] = NULL;

    return -1;
}

int FMR_open_dev(int idx)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).open_dev);
    ret = FMR_cbk_tbl(idx).open_dev(FM_DEV_NAME, &FMR_fd(idx));
    if (ret || FMR_fd(idx) < 0) {
        LOGE("%s failed, [fd=%d]\n", __func__, FMR_fd(idx));
        return ret;
    }

    LOGD("%s, [fd=%d]  [ret=%d]\n", __func__, FMR_fd(idx), ret);
    return ret;
}

int FMR_close_dev(int idx)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).close_dev);
    ret = FMR_cbk_tbl(idx).close_dev(FMR_fd(idx));
    LOGD("%s, [fd=%d] [ret=%d]\n", __func__, FMR_fd(idx), ret);
    return ret;
}

int FMR_pwr_up(int idx, int freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).pwr_up);

    LOGI("%s,[freq=%d]\n", __func__, freq);
    if (freq < fmr_data.cfg_data.low_band || freq > fmr_data.cfg_data.high_band) {
        LOGE("%s error freq: %d\n", __func__, freq);
        ret = -ERR_INVALID_PARA;
        return ret;
    }
    ret = FMR_cbk_tbl(idx).pwr_up(FMR_fd(idx), fmr_data.cfg_data.band, freq);
    if (ret) {
        LOGE("%s failed, [ret=%d]\n", __func__, ret);
    }
    fmr_data.cur_freq = freq;
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_pwr_down(int idx, int type)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).pwr_down);
    ret = FMR_cbk_tbl(idx).pwr_down(FMR_fd(idx), type);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_set_step(int idx, int step)
{
  int ret = 0;
  FMR_ASSERT(FMR_cbk_tbl(idx).set_step);
  ret = FMR_cbk_tbl(idx).set_step(FMR_fd(idx), step);
  LOGD("%s, [ret=%d]\n", __func__, ret);
  return ret;
}

int FMR_get_chip_id(int idx, int *chipid)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_chip_id);
    FMR_ASSERT(chipid);

    ret = FMR_cbk_tbl(idx).get_chip_id(FMR_fd(idx), chipid);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
        *chipid = -1;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_rssi(int idx, int *rssi)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_rssi);
    FMR_ASSERT(rssi);

    ret = FMR_cbk_tbl(idx).get_rssi(FMR_fd(idx), rssi);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
        *rssi = -1;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_snr(int idx, int *snr)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_snr);
    FMR_ASSERT(snr);

    ret = FMR_cbk_tbl(idx).get_snr(FMR_fd(idx), snr);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
        *snr = -1;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_tune(int idx, fm_seek_criteria_parm *parm)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_tune);
    FMR_ASSERT(parm);

    ret = FMR_cbk_tbl(idx).get_tune(FMR_fd(idx), parm);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_set_tune(int idx, fm_seek_criteria_parm *parm)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).set_tune);
    FMR_ASSERT(parm);

    ret = FMR_cbk_tbl(idx).set_tune(FMR_fd(idx), parm);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_audio(int idx, fm_audio_threshold_parm *parm)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_audio);
    FMR_ASSERT(parm);

    ret = FMR_cbk_tbl(idx).get_audio(FMR_fd(idx), parm);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_set_audio(int idx, fm_audio_threshold_parm *parm)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).set_audio);
    FMR_ASSERT(parm);

    ret = FMR_cbk_tbl(idx).set_audio(FMR_fd(idx), parm);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_rw_reg(int idx,  fm_reg_ctl_parm *fcp)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).rw_reg);
    FMR_ASSERT(fcp);

    ret = FMR_cbk_tbl(idx).rw_reg(FMR_fd(idx), fcp);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_ps(int idx, uint8_t **ps, int *ps_len)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_ps);
    FMR_ASSERT(ps);
    FMR_ASSERT(ps_len);
    ret = FMR_cbk_tbl(idx).get_ps(FMR_fd(idx), &fmr_data.rds, ps, ps_len);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_rt(int idx, uint8_t **rt, int *rt_len)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_rt);
    FMR_ASSERT(rt);
    FMR_ASSERT(rt_len);

    ret = FMR_cbk_tbl(idx).get_rt(FMR_fd(idx), &fmr_data.rds, rt, rt_len);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_bler(int idx, int *bler)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_bler);
    FMR_ASSERT(bler);

    ret = FMR_cbk_tbl(idx).get_bler(FMR_fd(idx), bler);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
        *bler = -1;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_tune(int idx, int freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).tune);

    ret = FMR_cbk_tbl(idx).tune(FMR_fd(idx), freq, fmr_data.cfg_data.band);
    if (ret) {
        LOGE("%s failed, [ret=%d]\n", __func__, ret);
    }
    fmr_data.cur_freq = freq;
    LOGD("%s, [freq=%d] [ret=%d]\n", __func__, freq, ret);
    return ret;
}

/*return: fm_true: desense, fm_false: not desene channel*/
fm_bool FMR_DensenseDetect(fm_s32 idx, fm_u16 ChannelNo, fm_s32 RSSI)
{
    fm_u8 bDesenseCh = 0;

    bDesenseCh = FMR_cbk_tbl(idx).desense_check(FMR_fd(idx), ChannelNo, RSSI);
    if (bDesenseCh == 1) {
        return fm_true;
    }
    return fm_false;
}

fm_bool FMR_SevereDensense(fm_u16 ChannelNo, fm_s32 RSSI)
{
    fm_s32 i = 0;
    struct fm_fake_channel_t *chan_info = fmr_data.cfg_data.fake_chan;

    //ChannelNo /= 10;
     LOGI(" SevereDensense[%d] RSSI[%d]\n", ChannelNo,RSSI);

    for (i=0; i<chan_info->size; i++) {
        if (ChannelNo == chan_info->chan[i].freq) {
            //if (RSSI < FM_SEVERE_RSSI_TH)
            if (RSSI < chan_info->chan[i].rssi_th) {
                LOGI(" SevereDensense[%d] RSSI[%d]\n", ChannelNo,RSSI);
                return fm_true;
            } else {
                break;
            }
        }
    }
    return fm_false;
}
#if (FMR_NOISE_FLOORT_DETECT==1)
/*return TRUE:get noise floor freq*/
fm_bool FMR_NoiseFloorDetect(fm_bool *rF, fm_s32 rssi, fm_s32 *F_rssi)
{
    if (rF[0] == fm_true) {
        if (rF[1] == fm_true) {
            F_rssi[2] = rssi;
            rF[2] = fm_true;
            return fm_true;
        } else {
            F_rssi[1] = rssi;
            rF[1] = fm_true;
        }
    } else {
        F_rssi[0] = rssi;
        rF[0] = fm_true;
    }
    return fm_false;
}
#endif

/*check the cur_freq->freq is valid or not
return fm_true : need check cur_freq->valid
         fm_false: check faild, should stop seek
*/
static fm_bool FMR_Seek_TuneCheck(int idx, fm_softmute_tune_t *cur_freq)
{
    int ret = 0;
    if (fmr_data.scan_stop == fm_true) {
        ret = FMR_tune(idx,fmr_data.cur_freq);
        LOGI("seek stop!!! tune ret=%d",ret);
        return fm_false;
    }
    ret = FMR_cbk_tbl(idx).soft_mute_tune(FMR_fd(idx), cur_freq);
    if (ret) {
        LOGE("soft mute tune, failed:[%d]\n",ret);
        cur_freq->valid = fm_false;
        return fm_true;
    }
    if (cur_freq->valid == fm_true)/*get valid channel*/ {
        if (FMR_DensenseDetect(idx, cur_freq->freq, cur_freq->rssi) == fm_true) {
            LOGI("desense channel detected:[%d] \n", cur_freq->freq);
            cur_freq->valid = fm_false;
            return fm_true;
        }
        if (FMR_SevereDensense(cur_freq->freq, cur_freq->rssi) == fm_true) {
            LOGI("sever desense channel detected:[%d] \n", cur_freq->freq);
            cur_freq->valid = fm_false;
            return fm_true;
        }
        LOGI("seek result freq:[%d] \n", cur_freq->freq);
    }
    return fm_true;
}
/*
check more 2 freq, curfreq: current freq, seek_dir: 1,forward. 0,backword
*/
static int FMR_Seek_More(int idx, fm_softmute_tune_t *validfreq, fm_u8 seek_dir, fm_u8 step, fm_u16 min_freq, fm_u16 max_freq)
{
    fm_s32 i;
    fm_softmute_tune_t cur_freq;
    cur_freq.freq = validfreq->freq;

    for (i=0; i<2; i++) {
        if (seek_dir)/*forward*/ {
            if (cur_freq.freq + step > max_freq) {
                return 0;
            }
            cur_freq.freq += step;
        } else/*backward*/ {
            if (cur_freq.freq - step < min_freq) {
                return 0;
            }
            cur_freq.freq -= step;
        }
        if (FMR_Seek_TuneCheck(idx, &cur_freq) == fm_true) {
            if (cur_freq.valid == fm_true) {
                if (cur_freq.rssi > validfreq->rssi) {
                    validfreq->freq = cur_freq.freq;
                    validfreq->rssi = cur_freq.rssi;
                    LOGI("seek cover last by freq=%d",cur_freq.freq);
                }
            }
        } else {
            return -1;
        }
    }
    return 0;
}

/*check the a valid channel
return -1 : seek fail
         0: seek success
*/
int FMR_seek_Channel(int idx, int start_freq, int min_freq, int max_freq, int band_channel_no, int seek_space, int dir, int *ret_freq, int *rssi_tmp)
{
    fm_s32 i, ret = 0;
    fm_softmute_tune_t cur_freq;

    if (dir == 1)/*forward*/ {
        for (i=((start_freq-min_freq)/seek_space+1); i<band_channel_no; i++) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----1",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid != fm_false) {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
        for (i=0; i<((start_freq-min_freq)/seek_space); i++) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----2",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid != fm_false) {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
    } else/*backward*/ {
        for (i=((start_freq-min_freq)/seek_space-1); i>=0; i--) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----3",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid != fm_false) {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
        for (i=(band_channel_no-1); i>((start_freq-min_freq)/seek_space); i--) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----4",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid != fm_false) {
                    if (FMR_Seek_More(idx, &cur_freq, dir,seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
    }

    *ret_freq = start_freq;
    return 0;
}

int FMR_seek(int idx, int start_freq, int dir, int *ret_freq, int spacing)
{
    fm_s32 ret = 0;
    fm_s32 band_channel_no = 0;
    fm_u8 seek_space = spacing;
    fm_u16 min_freq, max_freq;
    static fm_u32 seek_ret_cnt = 0;

    if ((start_freq < 7600) || (start_freq > 10800)) /*need replace by macro*/ {
        LOGE("%s error start_freq: %d\n", __func__, start_freq);
        return -ERR_INVALID_PARA;
    }

    if (fmr_data.cfg_data.band == FM_BAND_JAPAN)/* Japan band	   76MHz ~ 90MHz */ {
        band_channel_no = (9600-7600)/seek_space + 1;
        min_freq = 7600;
        max_freq = 9600;
    } else if (fmr_data.cfg_data.band == FM_BAND_JAPANW)/* Japan wideband  76MHz ~ 108MHz */ {
        band_channel_no = (10800-7600)/seek_space + 1;
        min_freq = 7600;
        max_freq = 10800;
    } else/* US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */ {
        band_channel_no = (10800-8750)/seek_space + 1;
        min_freq = 8750;
        max_freq = 10800;
    }

    if (seek_ret_cnt >= 8) {
        if (seek_ret_cnt == 9) {
            seek_ret_cnt = 0;
        }
        if (seek_ret_cnt == 8) {
            seek_ret_cnt += 1;
        }
        *ret_freq = start_freq;
        LOGE("hardware seek %d times, not seek valid freq\n",seek_ret_cnt);
        return ret;
    }

    fmr_data.scan_stop = fm_false;
    LOGD("seek start freq %d band_channel_no=[%d], seek_space=%d band[%d - %d] dir=%d\n", start_freq, band_channel_no,seek_space,min_freq,max_freq,dir);

    //ret = FMR_seek_Channel(idx, start_freq, min_freq, max_freq, band_channel_no, seek_space, dir, ret_freq, &rssi);

    int tmp_freq = (dir == 1)?  (start_freq + seek_space) : (start_freq - seek_space) ;
    ret = FMR_cbk_tbl(idx).seek(FMR_fd(idx), &tmp_freq, 0, !dir, 0);
    LOGE("hardware seek, ret: %d, current freq: %d\n",ret, tmp_freq);

    if (0 == ret) {
        if(tmp_freq != 0){
          *ret_freq = (tmp_freq);
          LOGE("hardware seek, ret: %d, ret freq: %d\n",ret, *ret_freq);
          // app will tune to freq later, so FMR_tune is not necessary.
          // ret = FMR_tune(idx, tmp_freq);
        }else {
          seek_ret_cnt++;
          *ret_freq = start_freq;
          LOGE("hardware seek channel none, seek_ret_cnt: %d, set start_freq: %d as tune value\n",seek_ret_cnt, *ret_freq);
          // app will tune to freq later, so FMR_tune is not necessary.
          // ret = FMR_tune(idx, start_freq/10);
        }
    }
    return ret;
}

int FMR_set_mute(int idx, int mute)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).set_mute)

    if ((mute < 0) || (mute > 1)) {
        LOGE("%s error param mute:  %d\n", __func__, mute);
    }

    ret = FMR_cbk_tbl(idx).set_mute(FMR_fd(idx), mute);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [mute=%d] [ret=%d]\n", __func__, mute, ret);
    return ret;
}

int FMR_is_rdsrx_support(int idx, int *supt)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).is_rdsrx_support);
    FMR_ASSERT(supt);

    ret = FMR_cbk_tbl(idx).is_rdsrx_support(FMR_fd(idx), supt);
    if (ret) {
        *supt = 0;
        LOGE("%s, failed\n", __func__);
    }
    LOGD("%s, [supt=%d] [ret=%d]\n", __func__, *supt, ret);
    return ret;
}

int FMR_Pre_Search(int idx)
{
    //avoid scan stop flag clear if stop cmd send before pre-search finish
    fmr_data.scan_stop = fm_false;
    FMR_ASSERT(FMR_cbk_tbl(idx).pre_search);
    FMR_cbk_tbl(idx).pre_search(FMR_fd(idx));
    return 0;
}

int FMR_Restore_Search(int idx)
{
    FMR_ASSERT(FMR_cbk_tbl(idx).restore_search);
    FMR_cbk_tbl(idx).restore_search(FMR_fd(idx));
    return 0;
}

int FMR_seek_Channels(int idx, int *scan_tbl, int *max_cnt, fm_s32 band_channel_no, fm_u16 Start_Freq, fm_u8 seek_space, fm_u8 NF_Space)
{
    fm_s32 ret = 0, Num = 0, i=0;
    fm_u32 ChannelNo = 0;
    fm_softmute_tune_t cur_freq;
    static struct fm_cqi SortData[CQI_CH_NUM_MAX];
    fm_u32 LastValidFreq = 0;
    fm_u32 seek_cnt = 0;

    memset(SortData, 0, CQI_CH_NUM_MAX*sizeof(struct fm_cqi));
    memset(&cur_freq, 0, sizeof(fm_softmute_tune_t));
    LOGI("band_channel_no=[%d], seek_space=%d, start freq=%d, NF_Space=%d\n", band_channel_no,seek_space,Start_Freq,NF_Space);

    cur_freq.freq = Start_Freq - seek_space;

    while(LastValidFreq < cur_freq.freq){
        if (fmr_data.scan_stop == fm_true) {
            FMR_Restore_Search(idx);
            // here may use to reset the freq
            ret = FMR_tune(idx, fmr_data.cur_freq);

            LOGI("scan stop!!! tune ret=%d",ret);
            break;
        }

        LastValidFreq = cur_freq.freq;

        cur_freq.freq += seek_space;

        ret = FMR_cbk_tbl(idx).seek(FMR_fd(idx), (int *)&cur_freq.freq, 0, 0, 0);
        LOGE("hardware seek, ret: %d, current freq: %d, last freq: %d\n",ret, cur_freq.freq, LastValidFreq);

        if (0 != ret && (LastValidFreq + seek_space == cur_freq.freq)) {
            LOGE("hardware scan fail:[%d]\n",seek_cnt);
            seek_cnt++;
            if(seek_cnt == 2){
                break;
            }
        }

        if (0 != ret || LastValidFreq >= cur_freq.freq) {
            LOGE("hardware scan stop:[%d]\n",ret);
            continue;
        }

        LOGI("FMR_scan_Channels try %d, freq: %d, is valid: %d ", i++, cur_freq.freq, cur_freq.valid);

        if (FMR_DensenseDetect(idx, cur_freq.freq, cur_freq.rssi) == fm_true) {
                LOGI("desense channel detected:[%d] \n", cur_freq.freq);
                continue;
        }
           
        if (FMR_SevereDensense(cur_freq.freq, cur_freq.rssi) == fm_true) {
                LOGI("FMR_SevereDensense channel detected:[%d] \n", cur_freq.freq);
                continue;
        }
        SortData[Num].ch = cur_freq.freq;
        SortData[Num].rssi = cur_freq.rssi;
        SortData[Num].reserve = 1;
        Num++;

        LOGI("Num++:[%d] \n", Num);
    }
	
    LOGI("get channel no.[%d] \n", Num);
    if (Num == 0)/*get nothing*/ {
        *max_cnt = 0;
        FMR_Restore_Search(idx);
        return -1;
    }
	
    for (i=0; i<Num; i++)/*debug*/ {
        LOGI("[%d]:%d \n", i,SortData[i].ch);
    }

    ChannelNo = 0;
    for (i=0; i<Num; i++) {
        if (SortData[i].reserve == 1) {
            scan_tbl[ChannelNo]=SortData[i].ch;
            ChannelNo++;
        }
    }
    *max_cnt=ChannelNo;

    LOGI("return channel no.[%d] \n", ChannelNo);
    return 0;
}

int FMR_scan(int idx, int *scan_tbl, int *max_cnt, int startFreq, int spacing)
{
    fm_s32 ret = 0;
    fm_s32 band_channel_no = 0;
    fm_u8 seek_space = spacing;
    fm_u16 Start_Freq = 8750;
    fm_u8 NF_Space = 41;

    //if (startFreq <= 10800 &&  startFreq >= 8750) Start_Freq = startFreq;

    if (fmr_data.cfg_data.band == FM_BAND_JAPAN)/* Japan band      76MHz ~ 90MHz */ {
        band_channel_no = (960-760)/seek_space + 1;
        Start_Freq = 7600;
        NF_Space = 400/seek_space;
    } else if (fmr_data.cfg_data.band == FM_BAND_JAPANW)/* Japan wideband  76MHZ ~ 108MHz */ {
        band_channel_no = (1080-760)/seek_space + 1;
        Start_Freq = 7600;
        NF_Space = 640/seek_space;
    } else/* US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */ {
        band_channel_no = (1080-875)/seek_space + 1;
        Start_Freq = 8750;
        NF_Space = 410/seek_space;
    }

    //  we use hardware seek instead of software tune when scan channels
    ret = FMR_seek_Channels(idx, scan_tbl, max_cnt, band_channel_no, Start_Freq, seek_space, NF_Space);

    return ret;
}

int FMR_stop_scan(int idx)
{
    int ret = -1;

    fmr_data.scan_stop = fm_true;

    ret = FMR_cbk_tbl(idx).stop_scan(FMR_fd(idx));
    if (ret) {
        LOGE("%s failed, [ret=%d]\n", __func__, ret);
    }

    LOGD("%s,  [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_turn_on_off_rds(int idx, int onoff)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).turn_on_off_rds)
    ret = FMR_cbk_tbl(idx).turn_on_off_rds(FMR_fd(idx), onoff);
    if (ret) {
        LOGE("%s, failed\n", __func__);
    }
    LOGD("%s, [onoff=%d] [ret=%d]\n", __func__, onoff, ret);
    return ret;
}

int FMR_read_rds_data(int idx, uint16_t *rds_status)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).read_rds_data);
    FMR_ASSERT(rds_status);

    ret = FMR_cbk_tbl(idx).read_rds_data(FMR_fd(idx), &fmr_data.rds, rds_status);
    /*if (ret) {
        LOGE("%s, get no event\n", __func__);
    }*/
    LOGD("%s, [status=%d] [ret=%d]\n", __func__, *rds_status, ret);
    return ret;
}

int FMR_active_af(int idx, uint16_t *ret_freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).active_af);
    FMR_ASSERT(ret_freq);
    ret = FMR_cbk_tbl(idx).active_af(FMR_fd(idx),
                                    &fmr_data.rds,
                                    fmr_data.cfg_data.band,
                                    0,
                                    ret_freq);
    if ((ret == 0) && (*ret_freq != fmr_data.cur_freq)) {
        fmr_data.cur_freq = *ret_freq;
        LOGI("active AF OK, new channel[freq=%d]\n", fmr_data.cur_freq);
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_ana_switch(int idx, int antenna)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).ana_switch);

//    if (fmr_data.cfg_data.short_ana_sup == true) {
        ret = FMR_cbk_tbl(idx).ana_switch(FMR_fd(idx), antenna);
        if (ret) {
            LOGE("%s failed, [ret=%d]\n", __func__, ret);
        }
//    } else {
//        LOGW("FM antenna switch not support!\n");
//        ret = -ERR_UNSUPT_SHORTANA;
//    }

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

static void killer(int sig) {

  LOGD("kill signal: %d, fd: %d", sig, FMR_fd(0));

  if (FMR_fd(0) >= 0)
      FMR_cbk_tbl(0).close_dev(FMR_fd(0));

  LOGD("kill exit");

  kill(getpid(), SIGKILL);
}

int FMR_set_region(int idx, int region)
{
    int ret = 0;
    fmr_data.cfg_data.band = region;
    FMR_ASSERT(FMR_cbk_tbl(idx).set_region)
    ret = FMR_cbk_tbl(idx).set_region(FMR_fd(idx),region);
    if (ret) {
        LOGE("%s, failed\n", __func__);
    }
    LOGD("%s, [region=%d] [ret=%d]\n", __func__, region, ret);
    return ret;
}


