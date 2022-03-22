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

typedef struct {
  /** Set to sizeof(wcn_vndor_interface_t) */
  size_t size;

  /*
   * Functions need to be implemented in Vendor libray (libwcn-vendor.so).
   */
  int (*init)(void);
  int (*get_sw_version)(char *buf, int *len);
  int (*get_hw_version)(char *buf, int *len);
  int (*get_loglevel)(char *buf, int *len);
  int (*set_loglevel)(char *buf, int len);
  int (*enable_armlog)(void);
  int (*disable_armlog)(void);
  int (*get_armlog_status)(void);
  int (*manual_assert)(void);
  int (*manual_dumpmem)(void);
  int (*reset)(bool);
  int (*get_reset_status)(void);
  int (*send_at_cmd)(char *buf, int len);
  /** Closes the interface */
  int (*cleanup)(void);
} wcn_vendor_interface_t;

#endif /* WCN_VENDOR_H */

