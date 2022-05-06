/******************************************************************************
 *
 *  Copyright (C) 2016 Spreadtrum Corporation
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

#define LOG_TAG "bt_chip_vendor"

#include <unistd.h>
#include <log/log.h>
#include <string.h>
#include <cutils/properties.h>
#include "sharkl3.h"
#include "bt_vendor_sprd.h"
#include "bt_vendor_sprd_hci.h"
#include "bt_hci_bdroid.h"
#include "conf.h"
#include "upio.h"

#define SYSFS_CHIPID_NODE "/sys/devices/platform/sipc-virt/sipc-virt:core@3/sipc-virt:core@3:sprd-mtty/chipid"
#define CONFIGURATION_22nm_FILE "connectivity_configure.22nm.ini"

// pskey file structure default value
static pskey_config_t sharkl3_pskey;

static const conf_entry_t sharkl3_table[] = {
    {"pskey_cmd", 0, 0, 0, 0},

    CONF_ITEM_TABLE(g_dbg_source_sink_syn_test_data, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_in_standby_supported, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_master_supported, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_slave_supported, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(default_ahb_clk, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(device_class, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(win_ext, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(g_aGainValue, 0, sharkl3_pskey, 6),
    CONF_ITEM_TABLE(g_aPowerValue, 0, sharkl3_pskey, 5),

    CONF_ITEM_TABLE(feature_set, 0, sharkl3_pskey, 16),
    CONF_ITEM_TABLE(device_addr, 0, sharkl3_pskey, 6),

    CONF_ITEM_TABLE(g_sys_sco_transmit_mode, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_sys_uart0_communication_supported, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(edr_tx_edr_delay, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(edr_rx_edr_delay, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(g_wbs_nv_117, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(is_wdg_supported, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(share_memo_rx_base_addr, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(g_wbs_nv_118, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_nbv_nv_117, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(share_memo_tx_packet_num_addr, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(share_memo_tx_data_base_addr, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(g_PrintLevel, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(share_memo_tx_block_length, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(share_memo_rx_block_length, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(share_memo_tx_water_mark, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(g_nbv_nv_118, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(uart_rx_watermark, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(uart_flow_control_thld, 0, sharkl3_pskey, 1),

    CONF_ITEM_TABLE(comp_id, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(pcm_clk_divd, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(br_edr_diff_reserved, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_aBRChannelpwrvalue, 0, sharkl3_pskey, 8),
    CONF_ITEM_TABLE(g_aEDRChannelpwrvalue, 0, sharkl3_pskey, 8),
    CONF_ITEM_TABLE(g_aLEPowerControlFlag, 0, sharkl3_pskey, 1),
    CONF_ITEM_TABLE(g_aLEChannelpwrvalue, 0, sharkl3_pskey, 8),
    CONF_ITEM_TABLE(g_central_or_peripheral, 0, sharkl3_pskey, 1),
    {0, 0, 0, 0, 0}
};


static void hw_core_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    char        *p_name, *p_tmp;
    uint8_t     *p, status;
    uint16_t    opcode, mode;

    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);
    STREAM_TO_UINT16(mode,p);
    STREAM_TO_UINT8(status,p);

    ALOGI("%s hw_core_cback response: [0x%04X, 0x%04X, 0x%02X]", __func__, opcode, mode, status);
    if (bt_vendor_cbacks)
    {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);
        if (status) {
#ifdef ANDROID_4_4_4
            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_BT_SUCCESS);
#else
            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
#endif
        } else if (status == 0){
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
        }
    }
    upio_set(UPIO_BT_WAKE, UPIO_DEASSERT, 0);
}

void hw_core_enable(unsigned char enable)
{
    HC_BT_HDR *p_buf = NULL;
    uint8_t *p;
    int i = 0;

    ALOGI("%s", __func__);

    /* Sending a HCI_RESET */
    if (bt_vendor_cbacks) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
        BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + START_STOP_CMD_SIZE);
    }

    if (p_buf) {
        ALOGI("hw_core_enable : %d", enable);
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = START_STOP_CMD_SIZE + 3;

        p = (uint8_t *)(p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_ENABLE_COMMMAND);
        *p++ = START_STOP_CMD_SIZE; /* parameter length */
        UINT16_TO_STREAM(p, DUAL_MODE);
        *p = enable ? ENABLE_BT : DISABLE_BT;
        upio_set(UPIO_BT_WAKE, UPIO_ASSERT, 0);
        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_VSC_ENABLE_COMMMAND, p_buf, hw_core_cback);
    } else {
        ALOGI("hw_pskey_send dont send pskey");
        if (bt_vendor_cbacks) {
            ALOGE("vendor lib hw_pskey_send aborted [no buffer]");
        }
    }
}
static void sharkl3_pskey_dump(void *arg)
{
    UNUSED(arg);
    pskey_config_t *p = &sharkl3_pskey;
    ALOGI("pskey_cmd: 0x%08X", p->pskey_cmd);

    ALOGI("g_dbg_source_sink_syn_test_data: 0x%02X",
          p->g_dbg_source_sink_syn_test_data);
    ALOGI("g_sys_sleep_in_standby_supported: 0x%02X",
          p->g_sys_sleep_in_standby_supported);
    ALOGI("g_sys_sleep_master_supported: 0x%02X",
          p->g_sys_sleep_master_supported);
    ALOGI("g_sys_sleep_slave_supported: 0x%02X", p->g_sys_sleep_slave_supported);

    ALOGI("default_ahb_clk: %d", p->default_ahb_clk);
    ALOGI("device_class: 0x%08X", p->device_class);
    ALOGI("win_ext: 0x%08X", p->win_ext);

    ALOGI("g_aGainValue: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
          p->g_aGainValue[0], p->g_aGainValue[1], p->g_aGainValue[2],
          p->g_aGainValue[3], p->g_aGainValue[4], p->g_aGainValue[5]);
    ALOGI("g_aPowerValue: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
          p->g_aPowerValue[0], p->g_aPowerValue[1], p->g_aPowerValue[2],
          p->g_aPowerValue[3], p->g_aPowerValue[4]);

    ALOGI(
        "feature_set(0~7): 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
        "0x%02X, 0x%02X",
        p->feature_set[0], p->feature_set[1], p->feature_set[2],
        p->feature_set[3], p->feature_set[4], p->feature_set[5],
        p->feature_set[6], p->feature_set[7]);
    ALOGI(
        "feature_set(8~15): 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
        "0x%02X, 0x%02X",
        p->feature_set[8], p->feature_set[9], p->feature_set[10],
        p->feature_set[11], p->feature_set[12], p->feature_set[13],
        p->feature_set[14], p->feature_set[15]);
    ALOGI("device_addr: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
          p->device_addr[0], p->device_addr[1], p->device_addr[2],
          p->device_addr[3], p->device_addr[4], p->device_addr[5]);

    ALOGI("g_sys_sco_transmit_mode: 0x%02X", p->g_sys_sco_transmit_mode);
    ALOGI("g_sys_uart0_communication_supported: 0x%02X",
          p->g_sys_uart0_communication_supported);
    ALOGI("edr_tx_edr_delay: %d", p->edr_tx_edr_delay);
    ALOGI("edr_rx_edr_delay: %d", p->edr_rx_edr_delay);

    ALOGI("g_wbs_nv_117 : 0x%04X", p->g_wbs_nv_117);

    ALOGI("is_wdg_supported: 0x%08X", p->is_wdg_supported);

    ALOGI("share_memo_rx_base_addr: 0x%08X", p->share_memo_rx_base_addr);
    ALOGI("g_wbs_nv_118 : 0x%04X", p->g_wbs_nv_118);
    ALOGI("g_nbv_nv_117 : 0x%04X", p->g_nbv_nv_117);

    ALOGI("share_memo_tx_packet_num_addr: 0x%08X",
          p->share_memo_tx_packet_num_addr);
    ALOGI("share_memo_tx_data_base_addr: 0x%08X",
          p->share_memo_tx_data_base_addr);

    ALOGI("g_PrintLevel: 0x%08X", p->g_PrintLevel);

    ALOGI("share_memo_tx_block_length: 0x%04X", p->share_memo_tx_block_length);
    ALOGI("share_memo_rx_block_length: 0x%04X", p->share_memo_rx_block_length);
    ALOGI("share_memo_tx_water_mark: 0x%04X", p->share_memo_tx_water_mark);
    ALOGI("g_nbv_nv_118 : 0x%04X", p->g_nbv_nv_118);

    ALOGI("uart_rx_watermark: %d", p->uart_rx_watermark);
    ALOGI("uart_flow_control_thld: %d", p->uart_flow_control_thld);
    ALOGI("comp_id: 0x%08X", p->comp_id);
    ALOGI("pcm_clk_divd : 0x%04X", p->pcm_clk_divd);

    ALOGI("br_edr_diff_reserved : 0x%04X", p->br_edr_diff_reserved);
    ALOGI("g_aBRChannelpwrvalue:0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
        p->g_aBRChannelpwrvalue[0], p->g_aBRChannelpwrvalue[1], p->g_aBRChannelpwrvalue[2], p->g_aBRChannelpwrvalue[3],
        p->g_aBRChannelpwrvalue[4], p->g_aBRChannelpwrvalue[5], p->g_aBRChannelpwrvalue[6], p->g_aBRChannelpwrvalue[7]);
    ALOGI("g_aEDRChannelpwrvalue:0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
        p->g_aEDRChannelpwrvalue[0], p->g_aEDRChannelpwrvalue[1], p->g_aEDRChannelpwrvalue[2], p->g_aEDRChannelpwrvalue[3],
        p->g_aEDRChannelpwrvalue[4], p->g_aEDRChannelpwrvalue[5], p->g_aEDRChannelpwrvalue[6], p->g_aEDRChannelpwrvalue[7]);
    ALOGI("g_aLEPowerControlFlag : 0x%08X", p->g_aLEPowerControlFlag);
    ALOGI("g_aLEChannelpwrvalue:0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X",
        p->g_aLEChannelpwrvalue[0], p->g_aLEChannelpwrvalue[1], p->g_aLEChannelpwrvalue[2], p->g_aLEChannelpwrvalue[3],
        p->g_aLEChannelpwrvalue[4], p->g_aLEChannelpwrvalue[5], p->g_aLEChannelpwrvalue[6], p->g_aLEChannelpwrvalue[7]);
    ALOGI("g_central_or_peripheral : 0x%04X", p->g_central_or_peripheral);
}

static int get_pskey_size(void)
{
    int size = 0;
    conf_entry_t *p_entry = (conf_entry_t *)&sharkl3_table;
    while (p_entry->conf_entry != NULL) {
        size += p_entry->len * p_entry->size;
        p_entry++;
    }
    ALOGD("get_pskey_size: %d", size);
    return size;
}

static void sharkl3_pskey_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *)p_mem;
    char buf[] = FW_DEFAULT_PROP;

    uint16_t opcode, node, year;
    uint8_t *p, status, month, day;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode, p);

    p = (uint8_t *)(p_evt_buf + 1) + FW_NODE_BYTE;
    STREAM_TO_UINT16(node, p);
    p = (uint8_t *)(p_evt_buf + 1) + FW_DATE_Y_BYTE;
    STREAM_TO_UINT16(year, p);
    p = (uint8_t *)(p_evt_buf + 1) + FW_DATE_M_BYTE;
    STREAM_TO_UINT8(month, p);
    p = (uint8_t *)(p_evt_buf + 1) + FW_DATE_D_BYTE;
    STREAM_TO_UINT8(day, p);

    ALOGI("Bluetooth Firmware Node: %04X Date: %04x-%02x-%02x", node, year, month,
          day);
    snprintf(buf, sizeof(buf), "%04x.%04x.%02x.%02x", node, year, month, day);

    if (bt_vendor_cbacks) {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
    hw_core_enable(1);
}

static void sharkl3_pskey_reload(pskey_config_t *pskey)
{
    int ret;
    unsigned int value;
    char property_buf[128] = {0};
    ret = property_get("persist.vendor.sys.bt.dsp.log", property_buf, "1");
    if (ret >= 0) {
        value = strtoul(property_buf, 0, 0) & 0xFFFFFFFF;
        ALOGI("persist.vendor.sys.bt.dsp.log %u -> %u", pskey->share_memo_tx_packet_num_addr, value);
        pskey->share_memo_tx_packet_num_addr = value;
    }
}

static int sharkl3_pskey_preload(void *arg)
{
    HC_BT_HDR *p_buf = NULL;
    uint8_t *p, *src;
    int i = 0;

    ALOGI("%s", __func__);

    UNUSED(arg);

    /* Sending a HCI_RESET */
    if (bt_vendor_cbacks) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                    BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + PSKEY_PREAMBLE_SIZE);
    }

    if (p_buf) {
        ALOGI("hw_pskey_send send pskey");
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + get_pskey_size();

        p = (uint8_t *)(p_buf + 1);
        UINT16_TO_STREAM(p, HCI_PSKEY);
        *p = PSKEY_PREAMBLE_SIZE; /* parameter length */

        p++;

        sharkl3_pskey_reload(&sharkl3_pskey);

        UINT8_TO_STREAM(p, sharkl3_pskey.g_dbg_source_sink_syn_test_data);
        UINT8_TO_STREAM(p, sharkl3_pskey.g_sys_sleep_in_standby_supported);
        UINT8_TO_STREAM(p, sharkl3_pskey.g_sys_sleep_master_supported);
        UINT8_TO_STREAM(p, sharkl3_pskey.g_sys_sleep_slave_supported);

        UINT32_TO_STREAM(p, sharkl3_pskey.default_ahb_clk);
        UINT32_TO_STREAM(p, sharkl3_pskey.device_class);
        UINT32_TO_STREAM(p, sharkl3_pskey.win_ext);

        for (i = 0; i < 6; i++) {
            UINT32_TO_STREAM(p, sharkl3_pskey.g_aGainValue[i]);
        }
        for (i = 0; i < 5; i++) {
            UINT32_TO_STREAM(p, sharkl3_pskey.g_aPowerValue[i]);
        }

        for (i = 0; i < 16; i++) {
            UINT8_TO_STREAM(p, sharkl3_pskey.feature_set[i]);
        }
        for (i = 0; i < 6; i++) {
            UINT8_TO_STREAM(p, sharkl3_pskey.device_addr[i]);
        }

        UINT8_TO_STREAM(p, sharkl3_pskey.g_sys_sco_transmit_mode);
        UINT8_TO_STREAM(p, sharkl3_pskey.g_sys_uart0_communication_supported);
        UINT8_TO_STREAM(p, sharkl3_pskey.edr_tx_edr_delay);
        UINT8_TO_STREAM(p, sharkl3_pskey.edr_rx_edr_delay);

        UINT16_TO_STREAM(p, sharkl3_pskey.g_wbs_nv_117);

        UINT32_TO_STREAM(p, sharkl3_pskey.is_wdg_supported);

        UINT32_TO_STREAM(p, sharkl3_pskey.share_memo_rx_base_addr);
        UINT16_TO_STREAM(p, sharkl3_pskey.g_wbs_nv_118);
        UINT16_TO_STREAM(p, sharkl3_pskey.g_nbv_nv_117);

        UINT32_TO_STREAM(p, sharkl3_pskey.share_memo_tx_packet_num_addr);
        UINT32_TO_STREAM(p, sharkl3_pskey.share_memo_tx_data_base_addr);

        UINT32_TO_STREAM(p, sharkl3_pskey.g_PrintLevel);

        UINT16_TO_STREAM(p, sharkl3_pskey.share_memo_tx_block_length);
        UINT16_TO_STREAM(p, sharkl3_pskey.share_memo_rx_block_length);
        UINT16_TO_STREAM(p, sharkl3_pskey.share_memo_tx_water_mark);
        UINT16_TO_STREAM(p, sharkl3_pskey.g_nbv_nv_118);

        UINT16_TO_STREAM(p, sharkl3_pskey.uart_rx_watermark);
        UINT16_TO_STREAM(p, sharkl3_pskey.uart_flow_control_thld);
        UINT32_TO_STREAM(p, sharkl3_pskey.comp_id);
        UINT16_TO_STREAM(p, sharkl3_pskey.pcm_clk_divd);

        UINT16_TO_STREAM(p, sharkl3_pskey.br_edr_diff_reserved);
         for (i = 0; i < 8; i++) {
            UINT32_TO_STREAM(p, sharkl3_pskey.g_aBRChannelpwrvalue[i]);
        }
        for (i = 0; i < 8; i++) {
            UINT32_TO_STREAM(p, sharkl3_pskey.g_aEDRChannelpwrvalue[i]);
        }
        UINT32_TO_STREAM(p, sharkl3_pskey.g_aLEPowerControlFlag);
        for (i = 0; i < 8; i++) {
            UINT16_TO_STREAM(p, sharkl3_pskey.g_aLEChannelpwrvalue[i]);
        }
        UINT16_TO_STREAM(p, sharkl3_pskey.g_central_or_peripheral);

        upio_set(UPIO_BT_WAKE, UPIO_ASSERT, 0);

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_PSKEY, p_buf, sharkl3_pskey_cback);
    } else {
        ALOGI("hw_pskey_send dont send pskey");
        if (bt_vendor_cbacks) {
            ALOGE("vendor lib hw_pskey_send aborted [no buffer]");
            return -1;
        }
    }
    return 0;
}

static void sharkl3_epilog_process() {
    hw_core_enable(0);
}

static int get_file_name(char *name_t) {
    int fd, ret;
    char file_name[128] = {0}, id_str[10] = {0};
    unsigned int id = 0;

    /* property check */
    ret = property_get(VENDOR_LIB_CONF_PROPERTY_PATH, file_name, VENDOR_LIB_CONF_PATH);
    if (ret <0) {
        ALOGE("get ini path from %s error", VENDOR_LIB_CONF_PROPERTY_PATH);
    }
    strcat(file_name, "/");

    /* chip id check */
    fd = open(SYSFS_CHIPID_NODE, O_RDONLY);
    if (fd < 0) {
        ALOGE("open %s faild", SYSFS_CHIPID_NODE);
        goto normal;
    }

    ret = read(fd, id_str, sizeof(id_str));
    close(fd);
    if (ret < 0) {
        ALOGE("read %s faild", SYSFS_CHIPID_NODE);
        goto normal;

    }

    id_str[ret - 1] = 0;
    id = strtoul(id_str, 0, 0) & 0xFFFFFFFF;

    ALOGD("%s ret: %d, id: %s[%d]", __func__, ret, id_str, id);

    if (id == WCN_SHARKL3_CHIP_22NM) {
        strcat(file_name, CONFIGURATION_22nm_FILE);
        ALOGI("%s, set ini file: %s", __func__, file_name);
        memcpy(name_t, file_name, strlen(file_name));
        return 0;
    }

normal:;

    strcat(file_name, VENDOR_LIB_CONF_PROPERTY_FILE);
    ALOGI("%s, set ini file: %s", __func__, file_name);
    memcpy(name_t, file_name, strlen(file_name));

    return 0;
}


static int sharkl3_init(void) {
    int ret;
    char filename[128] = {0};
    char property[128] = {0};

    ALOGI("%s", __func__);
    memset(&sharkl3_pskey, 0, sizeof(sharkl3_pskey));

    /*bug1883480,modify 22nm ini path*/
    get_file_name(filename);

    vnd_load_configure(filename, &sharkl3_table[0]);

    set_mac_address(sharkl3_pskey.device_addr);

    memset(property, 0 ,sizeof(property));
    ret = property_get("persist.vendor.sys.bt.non.ssp", property, "close");
    if (ret >= 0 && !strcmp(property, "open")) {
        ALOGI("### disable BT SSP function due to property setting ###");
        ALOGI("SSP setting pskey from : 0x%02x to: 0x%02x", sharkl3_pskey.feature_set[6],
              (sharkl3_pskey.feature_set[6] & 0xF7));
        sharkl3_pskey.feature_set[6] &= 0xF7;
    }

    return 0;
}
static bt_adapter_module_t marlin_module = {
    .name = "sharkl3",
    .pskey = &sharkl3_pskey,
    .tab = (conf_entry_t *)&sharkl3_table,
    .init = sharkl3_init,
    .pskey_preload = sharkl3_pskey_preload,
    .epilog_process = sharkl3_epilog_process,
    .pskey_dump = sharkl3_pskey_dump,
    .get_conf_file = NULL
};

const bt_adapter_module_t *get_adapter_module(void)
{
    return &marlin_module;
}
