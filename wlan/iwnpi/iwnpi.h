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

#ifndef __IWNPI_H
#define __IWNPI_H

#include <stdbool.h>
#include <endian.h>
#include <linux/socket.h>
#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "nlnpi.h"

#define ETH_ALEN            (6)
#define WLNPI_RES_BUF_LEN   (128)
#define NL_GENERAL_NPI_ID   (1022)

#define IWNPI_EXEC_TMP_FILE ("/mnt/vendor/iwnpi_exec_data.log")

struct nlnpi_state {
	struct nl_sock *nl_sock;
	int nlnpi_id;
};

struct cmd {
	const char *name;
	const char *args;
	const char *help;
	const enum nlnpi_commands cmd;
	int nl_msg_flags;

	/*
	 * The handler should return a negative error code,
	 * zero on success, 1 if the arguments were wrong
	 * and the usage message should and 2 otherwise.
	 */
	int (*handler)(struct nlnpi_state *state, struct nl_cb *cb,
		       struct nl_msg *msg, int argc, char **argv);
};

#define TOPLEVEL(_name, _args, _nlcmd, _flags, _handler, _help) \
  struct cmd __section##_##_name __attribute__((used))          \
      __attribute__((section("__cmd"))) = {                     \
          .name = (#_name),                                     \
          .args = (_args),                                      \
          .cmd = (_nlcmd),                                      \
          .nl_msg_flags = (_flags),                             \
          .handler = (_handler),                                \
          .help = (_help),                                      \
  }

#define SECTION(_name)                                 \
  struct cmd __section##_##_name __attribute__((used)) \
      __attribute__((section("__cmd"))) = {            \
          .name = (#_name),                            \
  }

#define DECLARE_SECTION(_name) extern struct cmd __section##_##_name;

extern int iwnpi_debug;

DECLARE_SECTION(set);
DECLARE_SECTION(get);

#endif /* __IWNPI_H */
