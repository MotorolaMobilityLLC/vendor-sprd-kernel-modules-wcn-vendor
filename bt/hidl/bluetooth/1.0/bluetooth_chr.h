/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <stdbool.h>

//#include "bt_types.h"
//#include "bt_target.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifndef SNOOP_DBG
    #define SNOOP_DBG TRUE
#endif

#if (SNOOP_DBG == TRUE)
    #define SNOOPD(param, ...) ALOGD("%s " param, __FUNCTION__, ## __VA_ARGS__)
#else
    #define SNOOPD(param, ...) {}
#endif

#ifdef SNOOPD_V
    #define SNOOPV(param, ...) ALOGD("%s " param, __FUNCTION__, ## __VA_ARGS__)
#else
    #define SNOOPV(param, ...) {}
#endif

#define SNOOPE(param, ...) ALOGE("%s " param, __FUNCTION__, ## __VA_ARGS__)

/********test framework struct*********/
//Where the event_content points to bt_chr_hardware_error_struct
typedef struct
{
    uint32_t hardware_err_code;
}bt_chr_hardware_error_struct;


typedef struct
{
    uint32_t ref_count;
    uint32_t event_id; // 23002
    uint32_t event_content_len;  //
    uint32_t version; //1
    char * event_content;
}bt_chr_ind_msg_struct;

typedef enum{
    WCN_BT_SUCCESS = 0,
    WCN_BT_ERROR,
}wcn_bt_status;

typedef enum{
    None_Error = 0,
    UART_OVERRUN,
    RX_PDU_MALFORMED,
    INVALID_DEVICE_LINK,
    MSS_FAIL,
    DEBUG_CLK_READ,
    STACK_CORRUPTION,
    UART_FRAMING,
    INVALID_LMP_LINK,
    INVALID_PICONET_LINK,
    INCORRECT_RADIO_VERSION,
    SCHEDULER_CONFIGURATION,
    INCORRECT_TABASCO_VERSION,
    INCORRECT_CTRLSTATE,
    OUT_OF_LMP_TIMERS,
    INCORRECT_SUPER_STATE,
    CORRUPTION_OF_LMP_TIMERS,
    CORRUPTION_OF_QUEUES,
    OUT_OF_LMP_QUEUE_BUFFERS,
    LMP_QUEUE_CORRUPTED,
    OUT_OF_LE_LLC_QUEUE_BUFFERS,
    LE_LLC_QUEUE_CORRUPTED,
}hardware_err_code;

/******************Externs*********************/
void bluetooth_chr_init(void);
//static void bluetooth_chr_thread(void* param);
void bluetooth_chr_cleanup(void);
void send_btchrmsg_hardware_error_to_framework(uint8_t *data);
int select_num_from_string(char *num_in_str);
char **explode(char sep, const char *str, int *size);
int set_cmd_format();
void recv_btchrmsg(uint8_t *data);
void wcn_chr_set_event(uint8_t mode_change, uint32_t set_event);
void wcn_chr_request_event(uint32_t set_event);
void wcn_chr_eventid_error_event();

/**********************************************/


//extern const btsnoop_sprd_t *btsnoop_sprd_for_raw_get_interface(void);
extern void *btsnoop_raw_start_up(void);
extern void *btsnoop_raw_shut_down(void);



#ifdef __cplusplus
}
#endif
