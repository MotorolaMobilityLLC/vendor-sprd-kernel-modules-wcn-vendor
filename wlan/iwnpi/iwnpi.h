/*
* SPDX-FileCopyrightText: 2021-2023 Unisoc (Shanghai) Technologies Co. Ltd
* SPDX-License-Identifier: GPL-2.0-only
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
