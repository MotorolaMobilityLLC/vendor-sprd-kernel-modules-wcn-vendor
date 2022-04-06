/******************************************************************************
 *
 *  Copyright (C) 2019 Unisoc Corporation
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
 *  Filename:      wcn_vendor.h
 *
 *  Description:   A wrapper header file of wcn_vendor.h
 *
 *                 Contains definitions specific for interfacing with Unisoc
 *                 WCN chipsets
 *
 ******************************************************************************/

#ifndef WCN_VENDOR_UNISOC_H
#define WCN_VENDOR_UNISOC_H



/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif


#ifndef LPM_COMBINE_SLEEP_MODE_AND_LPM
#define LPM_COMBINE_SLEEP_MODE_AND_LPM  1
#endif

#define WCND_SOCKET_CMD_ERROR_QUOTE  2

#define WCND_SOCKET_CMD_REPLY_LENGTH 255
#define WCND_SOCKET_CMD_CP2_VERSION          "at+spatgetcp2info"
#define WCND_SOCKET_CMD_ENABLE_CP2_LOG       "at+armlog=1"
#define WCND_SOCKET_CMD_DISABLE_CP2_LOG      "at+armlog=0"
#define WCND_SOCKET_CMD_CP2_LOG_STATUS       "at+armlog?"
#define WCND_SOCKET_CMD_CP2_LOG_STATUS_RSP   "+ARMLOG:"
#define WCND_SOCKET_CMD_GET_CP2_LOGLEVEL     "at+loglevel?"
#define WCND_SOCKET_CMD_SET_CP2_LOGLEVEL     "at+loglevel="
#define WCND_SOCKET_CMD_CP2_LOGLEVEL_RSP     "+LOGLEVEL:"
#define WCND_SOCKET_CMD_CP2_ASSERT           "at+spatassert=1"
#define WCND_SOCKET_CMD_CP2_DUMP             "dumpmem"
#define WCND_SOKCET_CMD_CP2_DUMP_ENABLE      "dump_enable"
#define WCND_SOCKET_CMD_CP2_DUMP_DISABLE     "dump_disable"
#define WCND_SOCKET_CMD_CP2_RESET_STATUS     "at+reset?"
#define WCND_SOCKET_CMD_CP2_RESET_STATUS_RSP "+RESET:"
#define WCND_SOCKET_CMD_CP2_RESET_ENABLE     "reset_enable"
#define WCND_SOCKET_CMD_CP2_RESET_DISABLE    "reset_disable"
#define WCND_SOCKET_CMD_CP2_RESET_DUMP       "reset_dump"
#define WCND_SOCKET_CMD_AT_PREFIX            "at+"

#define WCND_DUMP       0
#define WCND_RESET      1
#define WCND_RESET_DUMP 2

typedef struct {
	/* Set to sizeof(wcn_vndor_interface_t) */
	size_t size;

	/*
	 * Functions need to be implemented in Vendor libray (libwcn-vendor.so).
	 */
	int (*wcn_cmd_opt)(char *argv[], char *result,
			   int len, int *error_type);
} wcn_vendor_interface_t;

#endif /* WCN_VENDOR_H */

