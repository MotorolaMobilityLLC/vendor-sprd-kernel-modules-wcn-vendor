//
// Copyright 2016 The Android Open Source Project
// This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.
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
#include <string.h>
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

/*****************************************************************************
*  Local type definitions
*****************************************************************************/
//only for offload 
SessionType session_type = SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH;

enum BluetoothStreamState {
  DISABLED = 0,  // This stream is closing or set param "suspend=true"
  STANDBY,
  STARTING,
  STARTED,
  SUSPENDING,
  UNKNOWN,
};
BluetoothStreamState state_ = BluetoothStreamState::DISABLED;

// The maximum time to wait in std::condition_variable::wait_for()
constexpr unsigned int kMaxWaitingTimeMs = 4500;

//mutable std::mutex cv_mutex_;
std::mutex cv_mutex_;
std::condition_variable internal_cv_;

auto cookie_ = ::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined;

//function for a2dpoffload.c
extern "C"
{
  void stack_resp_cb(const BluetoothAudioStatus& status);
  void session_resp_cb(void);
  bool CondwaitState(BluetoothStreamState state);
  int audio_stream_open(void);
  int Start(void);
  int audio_stream_start(void);
  int Suspend(void);
  int audio_stream_suspend(void);
  void TearDown(void);
  int Stop(void);
  int audio_stream_stop(void);
  int audio_stream_close(void);
  bool audio_check_a2dp_ready(void);
  char* audio_get_codec_config(uint8_t *multicast_status, uint8_t *num_dev, enc_codec_t *codec_type);
  void clear_a2dp_suspend_flag(void);
  /* function in a2dpoffload.c
  audio_handoff_triggered
  audio_get_a2dp_sink_latency
  ldac_codec_parser
  audio_is_scrambling_enabled
  bt_audio_pre_init
  */
}

/*******************************************************************************
*
* Function         stack_resp_cb session_resp_cb
*
* Description      cbacks
*
* Returns          int64
*
******************************************************************************/
void ControlResultHandler(const BluetoothAudioStatus& status) {
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "control_result_cb: session_type=" << toString(session_type)
            << ", previous_state=" << previous_state
            << ", status=" << toString(status);

  switch (previous_state) {
    case BluetoothStreamState::STARTED:
      /* Only Suspend signal can be send in STARTED state*/
      if (status == BluetoothAudioStatus::RECONFIGURATION ||
          status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type)
                     << ", previous_state=" << previous_state;
      }
      break;
    case BluetoothStreamState::STARTING:
      if (status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STARTED;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::STANDBY;
      }
      break;
    case BluetoothStreamState::SUSPENDING:
      if (status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // It will be failed if the headset is disconnecting, and set to disable
        // to wait for re-init again
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::DISABLED;
      }
      break;
    default:
      LOG(ERROR) << "control_result_cb: unexpected status=" << toString(status)
                 << " for session_type=" << toString(session_type)
                 << ", previous_state=" << previous_state;
      return;
  }
  port_lock.unlock();
  internal_cv_.notify_all();
}

void SessionChangedHandler(void) {
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "session_changed_cb: session_type=" << toString(session_type)
            << ", previous_state=" << previous_state;
  state_ = BluetoothStreamState::DISABLED;
  port_lock.unlock();
  internal_cv_.notify_all();
}

/*******************************************************************************
*
* Function         CondwaitState
*
* Description      set wait time
*
* Returns          bool
*
******************************************************************************/
bool CondwaitState(BluetoothStreamState state) {
  bool retval;
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  switch (state) {
    case BluetoothStreamState::STARTING:
      LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
                   << " waiting for STARTED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          //[this] { return this->state_ != BluetoothStreamState::STARTING; });
          [] { return state_ != BluetoothStreamState::STARTING; });
      retval = retval && state_ == BluetoothStreamState::STARTED;
      LOG(INFO) << __func__ << ": retval=" << retval <<" status=" << state_;
      break;
    case BluetoothStreamState::SUSPENDING:
      LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
                   << " waiting for SUSPENDED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          //[this] { return this->state_ != BluetoothStreamState::SUSPENDING; });
          [] { return state_ != BluetoothStreamState::SUSPENDING; });
      retval = retval && state_ == BluetoothStreamState::STANDBY;
      LOG(INFO) << __func__ << ": retval=" << retval <<" status=" << state_;
      break;
    default:
      LOG(WARNING) << __func__ << ": session_type=" << toString(session_type)
                   << " waiting for KNOWN";
      return false;
  }
  return retval;  // false if any failure like timeout
}


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
  state_ = BluetoothStreamState::STANDBY;

  auto stack_resp_cb = [](uint16_t cookie, bool start_resp,
                                         const BluetoothAudioStatus& status) {
    ControlResultHandler(status);
  };
  
  auto session_resp_cb = [](uint16_t cookie) {
    SessionChangedHandler();
  };

  PortStatusCallbacks cbacks = {
      .control_result_cb_ = stack_resp_cb,
      .session_changed_cb_ = session_resp_cb,
  };

  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);
  if ( session_ptr != nullptr )
  {
    cookie_ = session_ptr->RegisterStatusCback(cbacks);
	if (cookie_ !=::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined){
	  //cookie_ is valid
      return true;
    }
    return false;
  } else
  {
    LOG(INFO) << __func__ << "GetSessionInstance session_ptr = NULL";
    return false;
  }
}

/*******************************************************************************
*
* Function         audio_stream_start
*
* Description
*
* Returns          int64
*
******************************************************************************/
int audio_stream_start(void) {
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
            << ", state=" << state_ << " request";
  bool retval = false;
  //HBS930 reply max 700ms
  int retry = 300;
  int i = 0;

  //if start when suspend not finish
  if (state_ == BluetoothStreamState::SUSPENDING) {
      retval = CondwaitState(BluetoothStreamState::SUSPENDING);
      if (!retval) {
        LOG(INFO) << __func__ << ": something error in Headset or air, suspend not finished in 4.5s, cannot start";
        return retval;
      }
  }

  if (state_ == BluetoothStreamState::STANDBY) {
    for(i=1; i<=retry; i++) {
      state_ = BluetoothStreamState::STARTING;
      //we accept remote start cmd when we are source, then suspend
      //if we start this time but "btif_av_stream_started_ready state==3(started), flag==LOCAL_SUSPEND_PENDING"
      //when encountering the above situations during software coding, audio will send out_write until successful
      if (Start()) {
        retval = CondwaitState(BluetoothStreamState::STARTING);
        if (retval) {
          break;
        }
      } else {
        LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
                 << ", state=" << state_ << " Hal fails";
      }
      //sleep can reduce audio start between accept remote start and send suspend
      usleep(5*1000);
      //LOG(INFO) << __func__ << ": sleep 5ms";
    }
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
              << ", retval=" << retval << " retry " << i << " times ";

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
              << ", state=" << state_ << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
               << ", state=" << state_ << " failure";
  }
  return retval;  // false if any failure like timeout
}

int Start(void) {
    std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

    if ( session_ptr != nullptr )
    {
      bool low_latency = false;
      return session_ptr->StartStream(low_latency);
    }
    return false;
}

/*******************************************************************************
*
* Function         audio_stream_stop
*
* Description      Google out_set_parameters in stream_apis.cc use stop in "A2dpSuspended"
*                  audio_extn_a2dp_set_parameters in a2dpoffload.c use suspend in "A2dpSuspended"
*                  so depend on Google code, we will change audio_stream_suspend and audio_stream_stop
* Returns          int64
*
******************************************************************************/
int audio_stream_stop(void) {
  LOG(INFO) << __func__ << ": will enter SuspendStream session_type=" << toString(session_type)
            << ", state=" << state_ << " request";

  //change device or disconnect device, audio will set parameters to close and out_standby
  //when excute close state change to DISABLE, do not need execute stop，so return(false is not fail)
  if (state_ == BluetoothStreamState::DISABLED) {
    return false;
  }

  bool retval = false;
  //if pause or disturb by another app, when start not finish
  if (state_ == BluetoothStreamState::STARTING) {
      retval = CondwaitState(BluetoothStreamState::STARTING);
      if (!retval) {
        LOG(INFO) << __func__ << ": something error in Headset or air, start not finished in 4.5s, cannot susupend";
        return retval;
      }
  }

  if (state_ == BluetoothStreamState::STARTED) {
    state_ = BluetoothStreamState::SUSPENDING;
    if (Suspend()) {
      retval = CondwaitState(BluetoothStreamState::SUSPENDING);
    } else {
      LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
              << ", state=" << state_ << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
               << ", state=" << state_ << " failure";
  }

  return retval;  // false if any failure like timeout
}

int Suspend(void) {
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    session_ptr->SuspendStream();
    return true;
  }
  return false;
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
void clear_a2dp_suspend_flag(void) {
  LOG(INFO) << __func__ << ": state=" << state_ << " stream param standby";
  if ( state_ == BluetoothStreamState::DISABLED) {
    state_ = BluetoothStreamState::STANDBY;
    LOG(INFO) << __func__ << ": refresh state=" << state_;
  }
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
int audio_stream_suspend(void) {
  LOG(INFO) << __func__ << ": will enter StopStream session_type=" << toString(session_type)
            << ", state=" << state_ << " request";

  bool retval = false;
  if (state_ == BluetoothStreamState::STARTED) {
    state_ = BluetoothStreamState::SUSPENDING;
    if (Stop()) {
      retval = CondwaitState(BluetoothStreamState::SUSPENDING);
    } else {
      LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type)
              << ", state=" << state_ << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type)
               << ", state=" << state_ << " failure";
  }

  state_ = BluetoothStreamState::DISABLED;
  return retval;  // false if any failure like timeout
}

int Stop(void) {
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    session_ptr->StopStream();
  }
  return true;
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
int audio_stream_close(void) {
  if (state_ != BluetoothStreamState::DISABLED) {
    audio_stream_suspend();
  }
  TearDown();
  LOG(INFO) << __func__ << ": state=" << state_
               << ", closed";
  return true;
}

void TearDown(void) {
  std::shared_ptr<BluetoothAudioSession> session_ptr = BluetoothAudioSessionInstance::GetSessionInstance(session_type);

  if ( session_ptr != nullptr )
  {
    session_ptr->UnregisterStatusCback(cookie_);
    LOG(INFO) << __func__ << "entered UnregisterStatusCback in BluetoothAudioSession";
  }
  cookie_ = ::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
