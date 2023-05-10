//
// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
/*****************************************************************************
*
*  Filename:      btaudio_offload_if.cc
*
*  Description:   Interface for Audio and BT Stack
*
*****************************************************************************/
#define LOG_TAG "btaudio_offload_if"

#include <stdint.h>
#include <stdio.h>

//...#include "libbase.h"
//...#include "libcutils.h"
//...#include "libfmq.h"
//...#include "libhidlbase.h"
//...#include "liblog.h"
//...#include "libutils.h"
//...#include "android.hardware.bluetooth.audio@2.0.h"
//...#include "libc++.h"
//...#include "libc.h"
//...#include "libm.h"
//...#include "libdl.h"
#include "BluetoothAudioSession.h"
#include "BluetoothAudioSessionControl.h"
#include "btaudio_offload_if.h"
#include <sys/types.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android/binder_manager.h>
#include <aidl/android/hardware/bluetooth/audio/SessionType.h>

using aidl_SbcChannelMode = ::aidl::android::hardware::bluetooth::audio::SbcChannelMode;
using aidl_SbcAllocMethod = ::aidl::android::hardware::bluetooth::audio::SbcAllocMethod;
using ::aidl::android::hardware::bluetooth::audio::SbcConfiguration;
SbcConfiguration aidl_OffLoadSbcParameters;

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

//function for a2dpoffload.c
extern "C"
{
  int audio_stream_open(void);
  int audio_stream_start(void);
  int audio_stream_suspend(void);
  int audio_stream_stop(void);
  int audio_stream_close(void);
  bool audio_check_a2dp_ready(void);
  char* audio_get_codec_config(uint8_t *multicast_status, uint8_t *num_dev, enc_codec_t *codec_type);
  //clear_a2dp_suspend_flag
  //stack_resp_cb;
  //session_resp_cb;
  /* function in a2dpoffload.c
  audio_handoff_triggered
  audio_get_a2dp_sink_latency
  ldac_codec_parser
  audio_is_scrambling_enabled
  bt_audio_pre_init
  */
}

/*****************************************************************************
*  Local type definitions
*****************************************************************************/
SessionType session_type = SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH;

enum class BluetoothStreamState : uint8_t {
  DISABLED = 0,  // This stream is closing or set param "suspend=true"
  STANDBY,
  STARTING,
  STARTED,
  SUSPENDING,
  UNKNOWN,
};

BluetoothStreamState state_;
/*******************************************************************************
*
* Function         cbacks
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
/*
PortStatusCallbacks cbacks = {
  .control_result_cb_ = stack_resp_cb,
  .session_changed_cb_ = session_resp_cb,
};
*/
/*******************************************************************************
*
* Function         audio_get_codec_config
*
* Description
*
*
* Returns          char
*
******************************************************************************/
char* audio_get_codec_config(uint8_t *multicast_status, uint8_t *num_dev, enc_codec_t *codec_type)
{

  char* func_status = "noconf";
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    if( multicast_status != nullptr && num_dev != nullptr && codec_type != nullptr)
    {
      LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
      AudioConfiguration codec_info = session_ptr->GetAudioConfig();

      CodecConfiguration sbc_codec_cfg = codec_info.get<AudioConfiguration::a2dpConfig>();
      aidl_OffLoadSbcParameters = sbc_codec_cfg.config.get<CodecConfiguration::CodecSpecific::sbcConfig>();

      std::unordered_map<std::string, std::string> return_params;
      std::string param;

      switch (aidl_OffLoadSbcParameters.channelMode) {
        case aidl_SbcChannelMode::MONO:
          param = "0x08";
          break;
        case aidl_SbcChannelMode::DUAL:
          param = "0x04";
          break;
        case aidl_SbcChannelMode::STEREO:
          param = "0x02";
          break;
        case aidl_SbcChannelMode::JOINT_STEREO:
          param = "0x01";
          break;
        default:
          param = "0x00";
          break;
      }
      return_params["ch_mode"] = param;

      switch (aidl_OffLoadSbcParameters.blockLength) {
        //SbcBlockLength::BLOCKS_4
        case 4:
          param = "0x80";
          break;
        //SbcBlockLength::BLOCKS_8
        case 8:
          param = "0x40";
          break;
        //SbcBlockLength::BLOCKS_12
        case 12:
          param = "0x20";
          break;
        //SbcBlockLength::BLOCKS_16
        case 16:
          param = "0x10";
          break;
        default:
          param = "0x00";
          break;
      }
      return_params["blocks"] = param;

      switch (aidl_OffLoadSbcParameters.numSubbands) {
        //SbcNumSubbands::SUBBAND_4
        case 4:
          param = "0x08";
          break;
        //SbcNumSubbands::SUBBAND_8
        case 8:
          param = "0x04";
          break;
        default:
          param = "0x00";
          break;
      }
      return_params["SubBands"] = param;

      switch (aidl_OffLoadSbcParameters.sampleRateHz) {
        //SampleRate::RATE_16000
        case 16000:
          param = "0x80";
          break;
        //SampleRate::RATE_44100
        case 44100:
          param = "0x20";
          break;
        //SampleRate::RATE_48000
        case 48000:
          param = "0x10";
          break;
        default:
          param = "0x00";
          break;
      }
      return_params["SamplingFreq"] = param;

      switch (aidl_OffLoadSbcParameters.allocMethod) {
        case aidl_SbcAllocMethod::ALLOC_MD_S:
          param = "0x02";
          break;
        case aidl_SbcAllocMethod::ALLOC_MD_L:
          param = "0x01";
          break;
        default:
          param = "0x00";
          break;
      }
      return_params["AllocMethod"] = param;

      {
        char str[10];
        sprintf(str,"%d",aidl_OffLoadSbcParameters.maxBitpool);
        return_params["Min_Bitpool"] = str;
        return_params["Max_Bitpool"] = str;
      }

      std::string result;
      for (const auto& ptr : return_params) {
        result += ptr.first + "=" + ptr.second + ";";
      }
      return strdup(result.c_str());
    }else
    {
      func_status = "param fail";
    }
  }else{
    func_status = "session error";
  }
  return func_status;
}

/*******************************************************************************
*
* Function         clear_a2dp_suspend_flag
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
//clear_a2dp_suspend_flag
//{
//}
/*******************************************************************************
*
* Function         audio_check_a2dp_ready
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
bool audio_check_a2dp_ready()
{
  bool a2dp_ready = false;

  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
    a2dp_ready = session_ptr->IsSessionReady();
    LOG(INFO) << __func__ << "entered IsSessionReady in BluetoothAudioSession";
  }
  LOG(INFO) << __func__ << "state = " << a2dp_ready;

  return a2dp_ready;
}

/*******************************************************************************
*
* Function         audio_stream_open
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
int audio_stream_open(void)
{
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
    //hal_audio_cfg = session_ptr->GetAudioConfig();
    //cookie_ = session_ptr->RegisterStatusCback(cbacks);
    LOG(INFO) << __func__ << "entered GetAudioConfig and RegisterStatusCback in BluetoothAudioSession";
    return 0;
  }else
  {
    LOG(INFO) << __func__ << "GetSessionInstance session_ptr = NULL";
    //hal_audio_cfg = AudioConfiguration(CodecConfiguration{});
    //cookie_ = kObserversCookieUndefined;
    return 1;
  }
}

/*******************************************************************************
*
* Function         audio_stream_start
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
int audio_stream_start(void)
{
  //...const char *a2dp_state;

  LOG(INFO) << __func__ << "aviliang entered";
  //...if ( out->common == NULL )
    //...a2dp_state = "UNKNOWN A2DP_STATE";
  //...else
    //...a2dp_state = out->common.state;
    //...LOG_I("btaudio_offload: audio_stream_start: state = %s", a2dp_state);
    //...pthread_mutex_lock(&dword_0);


  //...if (a2dp_state == AUDIO_A2DP_STATE_SUSPENDED || AUDIO_A2DP_STATE_STOPPING)
  //...{
    //...LOG_I("btaudio_offload: audio_stream_start, stream suspended or closing");
    //...pthread_mutex_unlock(&dword_0);
    //...status = -1;
  //...}
  //...else
  //...{
    //...if ((out->common.state == AUDIO_A2DP_STATE_STOPPED) || (out->common.state == AUDIO_A2DP_STATE_STANDBY))
    //...{

      std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

      if ( session_ptr != nullptr )
      {
        LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
        //...sprintf("btaudio_offload: audio_stream_start: state = %s", a2dp_state);
        bool low_latency;
        low_latency = false;
        session_ptr->StartStream(low_latency);
        LOG(INFO) << __func__ << "entered StartStream in BluetoothAudioSession";
      }

      //else


      //vendor/sprd/modules/wcn/vendor/bt/hidl/bluetooth/audio/aidl/default/BluetoothAudioProvider.cpp -- binderDiedCallbackAidl
      //...LOG_I("btaudio_offload: audio_stream_start, client has died");
      //...status = 0;
    //...}
    //there is no AUDIO_A2DP_STATE_STARTING in packages/modules/Bluetooth/system/audio_a2dp_hw/src/audio_a2dp_hw.cc -- out_write
    //...else if ((out->common.state != AUDIO_A2DP_STATE_STARTED))
    //...{
      //...status = -1;
      //...LOG_I("btaudio_offload: audio_stream_start, stream not in stopped or standby");
    //...}
    //...result = pthread_mutex_unlock(&dword_0);
  //...}
  //...if (  )
    //...result = ;
  //...return result;
  //return 0 is used as test
  return 0;
}

/*******************************************************************************
*
* Function         audio_stream_suspend
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
 int audio_stream_suspend(void)
 {
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
    session_ptr->SuspendStream();
    LOG(INFO) << __func__ << "entered SuspendStream in BluetoothAudioSession";
  }
  return 0;
 }

/*******************************************************************************
*
* Function         audio_stream_stop
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
int audio_stream_stop(void)
{
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
    session_ptr->StopStream();
    LOG(INFO) << __func__ << "entered StopStream in BluetoothAudioSession";
  }
  return 0;
}

/*******************************************************************************
*
* Function         audio_stream_close
*
* Description
*
*
* Returns          int64
*
******************************************************************************/
int audio_stream_close(void)
{
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    LOG(INFO) << __func__ << "GetSessionInstance get session_ptr != NULL";
    //session_ptr->UnregisterStatusCback(cookie_);
    LOG(INFO) << __func__ << "entered UnregisterStatusCback in BluetoothAudioSession";
  }
  return 0;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
