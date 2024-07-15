/*
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2023-2024 Unisoc (Shanghai) Technologies Co., Ltd
 *
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 *
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef __LINUX_NLNPI_H
#define __LINUX_NLNPI_H

#include <linux/types.h>

/**
 * enum nlnpi_commands - supported nlnpi commands
 *
 * @NLNPI_CMD_UNSPEC: unspecified command to catch errors
 *
 * @NLNPI_CMD_START: Start NPI operation, no parameters.
 * @NLNPI_CMD_STOP: Stop NPI operation.
 *
 * @NLNPI_CMD_MAX: highest used command number
 * @__NLNPI_CMD_AFTER_LAST: internal use
 */
enum nlnpi_commands {
	/* don't change the order or add anything between, this is ABI! */
	NLNPI_CMD_UNSPEC,
	NLNPI_CMD_START,
	NLNPI_CMD_STOP,
	NLNPI_CMD_SET_MAC,
	NLNPI_CMD_GET_MAC,
	NLNPI_CMD_SET_CHANNEL,
	NLNPI_CMD_GET_CHANNEL,
	NLNPI_CMD_GET_RSSI,
	NLNPI_CMD_SET_TX_MODE,
	NLNPI_CMD_SET_RATE,
	NLNPI_CMD_GET_RATE,  //10
	NLNPI_CMD_TX_START,
	NLNPI_CMD_TX_STOP,
	NLNPI_CMD_RX_START,
	NLNPI_CMD_RX_STOP,
	NLNPI_CMD_SET_TX_POWER,
	NLNPI_CMD_GET_TX_POWER,
	NLNPI_CMD_SET_TX_COUNT,
	NLNPI_CMD_GET_RX_OK_COUNT,
	NLNPI_CMD_GET_RX_ERR_COUNT,
	NLNPI_CMD_CLEAR_RX_COUNT,  // 20
	NLNPI_CMD_GET_REG,
	NLNPI_CMD_SET_REG,
	NLNPI_CMD_SET_DEBUG,
	NLNPI_CMD_GET_DEBUG,
	NLNPI_CMD_SET_SBLOCK,
	NLNPI_CMD_GET_SBLOCK,

	/* add new commands above here */
	NLNPI_CMD_SIN_WAVE,
	NLNPI_CMD_LNA_ON,
	NLNPI_CMD_LNA_OFF,
	NLNPI_CMD_GET_LNA_STATUS,  // 30
	NLNPI_CMD_SPEED_UP,
	NLNPI_CMD_SPEED_DOWN,

	/*for samsung hardware test interface */
	NLNPI_CMD_SET_LONG_PREAMBLE,
	NLNPI_CMD_SET_SHORT_GUARD_INTERVAL,
	NLNPI_CMD_SET_BURST_INTERVAL,
	NLNPI_CMD_SET_PAYLOAD,
	NLNPI_CMD_GET_PAYLOAD,

	NLNPI_CMD_SET_PREAMBLE_TYPE,
	NLNPI_CMD_GET_PREAMBLE_TYPE,
	NLNPI_CMD_GET_TX_MODE,  // 40
	NLNPI_CMD_SET_BAND,
	NLNPI_CMD_GET_BAND,
	NLNPI_CMD_SET_BANDWIDTH,
	NLNPI_CMD_GET_BANDWIDTH,
	NLNPI_CMD_SET_SHORTGI,
	NLNPI_CMD_GET_SHORTGI,
	NLNPI_CMD_SET_MACFILTER,
	NLNPI_CMD_GET_MACFILTER,
	NLNPI_CMD_SET_PKTLEN,
	NLNPI_CMD_GET_PKTLEN,  // 50

	/* used to define NLNPI_CMD_MAX below */
	__NLNPI_CMD_AFTER_LAST,
	NLNPI_CMD_MAX = __NLNPI_CMD_AFTER_LAST - 1
};

/**
 * enum nlnpi_attrs - nlnpi netlink attributes
 *
 * @NLNPI_ATTR_UNSPEC: unspecified attribute to catch errors
 *
 * @NLNPI_ATTR_MAX: highest attribute number currently defined
 * @__NLNPI_ATTR_AFTER_LAST: internal use
 */
enum nlnpi_attrs {
	/* don't change the order or add anything between, this is ABI! */
	NLNPI_ATTR_UNSPEC,

	NLNPI_ATTR_IFINDEX,
	NLNPI_ATTR_REPLY_STATUS,
	NLNPI_ATTR_REPLY_DATA,
	NLNPI_ATTR_GET_NO_ARG,
	NLNPI_ATTR_START,
	NLNPI_ATTR_STOP,
	NLNPI_ATTR_TX_START,
	NLNPI_ATTR_TX_STOP,
	NLNPI_ATTR_RX_START,
	NLNPI_ATTR_RX_STOP,  // 10
	NLNPI_ATTR_TX_MODE,
	NLNPI_ATTR_TX_COUNT,
	NLNPI_ATTR_RSSI,
	NLNPI_ATTR_GET_RX_COUNT,
	NLNPI_ATTR_SET_RX_COUNT,
	NLNPI_ATTR_GET_TX_POWER,
	NLNPI_ATTR_SET_TX_POWER,
	NLNPI_ATTR_GET_CHANNEL,
	NLNPI_ATTR_SET_CHANNEL,
	NLNPI_ATTR_SET_RATE,  // 20
	NLNPI_ATTR_GET_RATE,
	NLNPI_ATTR_SET_MAC,

	/* all get attr is arg len */
	NLNPI_ATTR_GET_MAC,
	NLNPI_ATTR_SET_REG,
	NLNPI_ATTR_GET_REG,
	NLNPI_ATTR_GET_REG_ARG,
	NLNPI_ATTR_SET_DEBUG,
	NLNPI_ATTR_GET_DEBUG,
	NLNPI_ATTR_GET_DEBUG_ARG,
	NLNPI_ATTR_SBLOCK_ARG,  // 30

	/* add attributes here, update the policy in itm_nlnpi.c */
	NLNPI_ATTR_SIN_WAVE,
	NLNPI_ATTR_LNA_ON,
	NLNPI_ATTR_LNA_OFF,
	NLNPI_ATTR_GET_LNA_STATUS,
	NLNPI_ATTR_SPEED_UP,
	NLNPI_ATTR_SPEED_DOWN,

	/*for samsung hardware test interface */
	NLNPI_ATTR_SET_LONG_PREAMBLE,
	NLNPI_ATTR_SET_SHORT_GUARD_INTERVAL,
	NLNPI_ATTR_SET_BURST_INTERVAL,
	NLNPI_ATTR_SET_PAYLOAD,  // 40
	NLNPI_ATTR_GET_PAYLOAD,

	NLNPI_ATTR_SET_PREAMBLE_TYPE,
	NLNPI_ATTR_GET_PREAMBLE_TYPE,
	NLNPI_ATTR_GET_TX_MODE,
	NLNPI_ATTR_SET_BAND,
	NLNPI_ATTR_GET_BAND,
	NLNPI_ATTR_SET_BANDWIDTH,
	NLNPI_ATTR_GET_BANDWIDTH,
	NLNPI_ATTR_SET_SHORTGI,
	NLNPI_ATTR_GET_SHORTGI,  // 50
	NLNPI_ATTR_SET_MACFILTER,
	NLNPI_ATTR_GET_MACFILTER,
	NLNPI_ATTR_SET_PKTLEN,
	NLNPI_ATTR_GET_PKTLEN,

	__NLNPI_ATTR_AFTER_LAST,
	NLNPI_ATTR_MAX = __NLNPI_ATTR_AFTER_LAST - 1
};

#endif /* __LINUX_NLNPI_H */
