/******************************************************************************
 *
 *  Copyright (C) 2017 Spreadtrum Corporation
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

/******************************************************************************
 *
 *  Filename:      bt_vendor_brcm.c
 *
 *  Description:   Spreadtrum vendor specific library implementation
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"

#include <signal.h>
#include <unistd.h>
#include <log/log.h>
#include <string.h>
#include "bt_vendor_sprd.h"
#include "upio.h"
#include "userial_vendor.h"
#include "comm.h"
#include "sitm.h"

#include "bt_vendor_lib.h"
#include "bt_vendor_sprd_hci.h"
#include "bt_vendor_sprd_ssp.h"
//#include "btsnoop_sprd.h"
#include "bdroid_buildcfg.h"
#include "bt_hci_bdroid.h"

#ifndef BTVND_DBG
#define BTVND_DBG TRUE
#endif

#if (BTVND_DBG == TRUE)
#define BTVNDDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTVNDDBG(param, ...) {}
#endif

#define CASE_RETURN_STR(const) case const: return #const;

#define HCI_SET_SAR 0xFCEA
#define CHANNEL_BIT_MOVE(n)  (0x01 << n)
#define CHANNEL_MAX_BIT 7
#define HCI_SPRD_SET_CHANNEL_POWER_SIZE 9
int8_t set_power_count[8] = {0};

/******************************************************************************
**  Externs
******************************************************************************/

int hw_preload_pskey(void* arg);
uint8_t hw_lpm_enable(uint8_t turn_on);
uint32_t hw_lpm_get_idle_timeout(void);
void hw_lpm_set_wake_state(uint8_t wake_assert);
void vnd_load_conf(const char* p_path);
#if (HW_END_WITH_HCI_RESET == TRUE)
void hw_epilog_process(void);
#endif

/******************************************************************************
**  Variables
******************************************************************************/

bt_vendor_callbacks_t* bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/******************************************************************************
**  Local type definitions
******************************************************************************/

/******************************************************************************
**  Static Variables
******************************************************************************/

static const tUSERIAL_CFG userial_init_cfg = {
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_3M
};

static const bt_adapter_module_t* adapter_module;

/******************************************************************************
**  Functions
******************************************************************************/

/*****************************************************************************
**
**   BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS
**
*****************************************************************************/

static void terminate(int sig)
{
    ALOGD("terminate: %d", sig);
    userial_vendor_close();
    usleep(500 * 1000);
    ALOGD("terminate exit");
    upio_set(UPIO_BT_WAKE, UPIO_DEASSERT, 0);
    kill(getpid(), SIGKILL);
}

static int init(const bt_vendor_callbacks_t* p_cb,
                unsigned char* local_bdaddr)
{
    ALOGI("init");

    if (p_cb == NULL) {
        ALOGE("init failed with no user callbacks!");
        return -1;
    }

    userial_vendor_init();
    upio_init();
    char value[92] = {'\0'};
    property_get(BUILD_TYPE_PROP_KEY, value, "");
    if (strstr(value,USER_DEBUG_VERSION_STR)) {
        sprd_vendor_hci_init();
    }

    adapter_module = get_adapter_module();

    if (adapter_module->init != NULL) {
        adapter_module->init();
    }

    ALOGD("%s start up", adapter_module->name);

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t*)p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);

    signal(SIGINT, terminate);
    return 0;
}

static const char* dump_opcode(int opcode)
{
    switch (opcode) {
        CASE_RETURN_STR(BT_VND_OP_POWER_CTRL)
        CASE_RETURN_STR(BT_VND_OP_FW_CFG)
        CASE_RETURN_STR(BT_VND_OP_SCO_CFG)
        CASE_RETURN_STR(BT_VND_OP_USERIAL_OPEN)
        CASE_RETURN_STR(BT_VND_OP_USERIAL_CLOSE)
        CASE_RETURN_STR(BT_VND_OP_GET_LPM_IDLE_TIMEOUT)
        CASE_RETURN_STR(BT_VND_OP_LPM_SET_MODE)
        CASE_RETURN_STR(BT_VND_OP_LPM_WAKE_SET_STATE)
        CASE_RETURN_STR(BT_VND_OP_SET_AUDIO_STATE)
        CASE_RETURN_STR(BT_VND_OP_EPILOG)
        CASE_RETURN_STR(BT_VND_OP_A2DP_OFFLOAD_START)
        CASE_RETURN_STR(BT_VND_OP_A2DP_OFFLOAD_STOP)
#if (defined(SPRD_FEATURE_VND_OP_EVENT) && SPRD_FEATURE_VND_OP_EVENT == TRUE)
        CASE_RETURN_STR(BT_VND_OP_EVENT_CALLBACK)
#endif
        CASE_RETURN_STR(BT_VND_OP_SET_POWER)
    default:
        return "unknown status code";
    }
}

void sprd_bt_power_ctrl(unsigned char on)
{
    if (on == BT_VND_PWR_ON) {
        upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
    } else if (on == UPIO_BT_POWER_OFF) {
        upio_set_bluetooth_power(UPIO_BT_POWER_ON);
    }
}

void sprd_bt_lpm_wake_up(void)
{
    upio_set(UPIO_LPM_MODE, UPIO_ASSERT, 0);
}

int sprd_bt_bqb_init(void)
{
    int fd;
    fd = userial_vendor_open((tUSERIAL_CFG*)&userial_init_cfg);
    return fd;
}

static int sprd_set_channel_power(uint8_t* parameters) {
    int move_count = 0;

    uint8_t type  = *parameters;
    uint8_t channel= *(parameters+1);
    int8_t power = (int8_t)*(parameters+2);
    int8_t temp_power = power;

    uint8_t command[HCI_SPRD_SET_CHANNEL_POWER_SIZE];
    uint8_t* command_ptr = command;
    memset(command, 0, HCI_SPRD_SET_CHANNEL_POWER_SIZE);

    ALOGI("%s set power value: [channel:%d,type:%d,power:%d]", __func__, channel, type, power);
    channel &= 0xff;
    type &= 0x0f;

    /* power check */
    if (type) {
        if (temp_power > 0) {
            for (move_count = 0; move_count <= CHANNEL_MAX_BIT; move_count++) {
                if (channel & CHANNEL_BIT_MOVE(move_count)) {
                    for (temp_power = power;temp_power > 0;temp_power--) {
                        set_power_count[move_count] ++;
                        ALOGD("%s temp_power_reduce set count:%d,count:%d", __func__, set_power_count[move_count],move_count);
                    }
                }
            }
        } else if (temp_power < 0) {
            for (move_count = 0; move_count <= CHANNEL_MAX_BIT; move_count++) {
                if (channel & CHANNEL_BIT_MOVE(move_count)) {
                    for (temp_power = power;temp_power < 0;temp_power++) {
                        if (temp_power + set_power_count[move_count] < 0 ) {
                            ALOGD("%s invalid power range set count:%d,count:%d", __func__, set_power_count[move_count],move_count);
                            return -1;
                        } else {
                            set_power_count[move_count] --;
                        }
                        ALOGD("%s temp_power_add set count:%d,count:%d", __func__, set_power_count[move_count],move_count);
                    }
                }
            }
        } else {
            ALOGD("%s: set power is 0", __func__);
        }
    }

    /* fill command */
    UINT8_TO_STREAM(command_ptr, type);
    for (move_count = 0; move_count <= CHANNEL_MAX_BIT; move_count++) {
        if (channel & CHANNEL_BIT_MOVE(move_count)) {
            INT8_TO_STREAM(command_ptr, power);
        } else {
            INT8_TO_STREAM(command_ptr, 0);
        }
    }

    /* send command */
    HC_BT_HDR *p_buf = NULL;
    uint8_t *p;
    if (bt_vendor_cbacks) {
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE +
                                                           HCI_CMD_PREAMBLE_SIZE + HCI_SPRD_SET_CHANNEL_POWER_SIZE);
    }

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_SPRD_SET_CHANNEL_POWER_SIZE;

        p = (uint8_t *)(p_buf + 1);
        UINT16_TO_STREAM(p, HCI_SET_SAR);
        *p++ = HCI_SPRD_SET_CHANNEL_POWER_SIZE; /* parameter length */
        memcpy(p,command,HCI_SPRD_SET_CHANNEL_POWER_SIZE);

        for (int temp_count = 0; temp_count < HCI_SPRD_SET_CHANNEL_POWER_SIZE; temp_count++) {
            ALOGD("%s,vendor command:%d\n", __func__, p[temp_count]);
        }

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_SET_SAR, p_buf, NULL);
        ALOGD("%s,send command finish", __func__);
    }

    return 0;
}

/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void* param)
{
    int retval = 0;

    BTVNDDBG("op for %s", dump_opcode(opcode));

    switch (opcode) {
    case BT_VND_OP_POWER_CTRL: /*{
        int* state = (int*)param;
        if (*state == BT_VND_PWR_OFF) {
            upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
        } else if (*state == BT_VND_PWR_ON) {
            upio_set_bluetooth_power(UPIO_BT_POWER_ON);
        }
    }*/
    break;

    case BT_VND_OP_FW_CFG: {
#if (BT_VND_STACK_PRELOAD == TRUE)
        hw_preload_pskey(NULL);
#else
        bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
#endif
    }
    break;

    case BT_VND_OP_SCO_CFG: {
        bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
        retval = 0;
    }
    break;

    case BT_VND_OP_USERIAL_OPEN: {
        int(*fd_array)[] = (int(*)[])param;
        int fd, idx;
        fd = userial_vendor_open((tUSERIAL_CFG*)&userial_init_cfg);
#if (BT_VND_STACK_PRELOAD == FALSE)
        retval = hw_preload_pskey(&fd);
        if (retval < 0) {
            BTVNDDBG("preload pskey failed");
            userial_vendor_close();
            fd = -1;
        }
#endif

#if (BT_SITM_SERVICE == TRUE)
        fd = sitm_server_start_up(fd);
#endif

        if (fd != -1) {
            for (idx = 0; idx < CH_MAX; idx++) (*fd_array)[idx] = fd;

            retval = 1;
        }
        /* retval contains numbers of open fd of HCI channels */
    }
    break;

    case BT_VND_OP_USERIAL_CLOSE: {
        userial_vendor_close();
#if (BT_SITM_SERVICE == TRUE)
        sitm_server_shut_down();
#endif
        upio_set(UPIO_BT_WAKE, UPIO_DEASSERT, 0);
    }
    break;

    case BT_VND_OP_GET_LPM_IDLE_TIMEOUT: {
        uint32_t* timeout_ms = (uint32_t*)param;
        *timeout_ms = hw_lpm_get_idle_timeout();
    }
    break;

    case BT_VND_OP_LPM_SET_MODE: {
        uint8_t* mode = (uint8_t*)param;
        retval = hw_lpm_enable(*mode);
    }
    break;

    case BT_VND_OP_LPM_WAKE_SET_STATE: {
        uint8_t* state = (uint8_t*)param;
        uint8_t wake_assert = (*state == BT_VND_LPM_WAKE_ASSERT) ? TRUE : FALSE;

        hw_lpm_set_wake_state(wake_assert);
    }
    break;

    case BT_VND_OP_SET_AUDIO_STATE: {
        retval = -1;
    }
    break;

    case BT_VND_OP_EPILOG: {
#if (HW_END_WITH_HCI_RESET == FALSE)
        if (bt_vendor_cbacks) {
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
        }
#else
        hw_epilog_process();
#endif
    }
    break;

    case BT_VND_OP_A2DP_OFFLOAD_START:
    case BT_VND_OP_A2DP_OFFLOAD_STOP:
    break;

    case BT_VND_OP_SET_POWER: {
        retval = sprd_set_channel_power((uint8_t*)param);
    }
    break;

#if (defined(SPRD_FEATURE_VND_OP_EVENT) && SPRD_FEATURE_VND_OP_EVENT == TRUE)
    case BT_VND_OP_EVENT_CALLBACK: {
        uint8_t *p = (uint8_t*)param;
        BT_HDR *buf = (BT_HDR*)(p + 2);
        buf->len = buf->len - 2;
        sprd_vse_cback(buf->len, buf->data);
    }
    break;
#endif

    }
    return retval;
}

/** Closes the interface */
static void cleanup(void)
{
    BTVNDDBG("cleanup");

    upio_cleanup();

    bt_vendor_cbacks = NULL;

    sprd_vendor_hci_cleanup();

}


// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup,
};
