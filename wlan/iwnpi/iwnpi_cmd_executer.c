/*
* SPDX-FileCopyrightText: 2021-2023 Unisoc (Shanghai) Technologies Co. Ltd
* SPDX-License-Identifier: GPL-2.0-only
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <android/log.h>
#include <log/log.h>

#include "nlnpi.h"
#include "iwnpi.h"

static const char *argv0 = "iwnpi";
int iwnpi_debug = 0;

static int cmd_size;

extern struct cmd __start___cmd;
extern struct cmd __stop___cmd;

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG  ("IWNPI_ENG")

#define OK_STR      ("OK")
#define FAIL_STR    ("FAIL")

#define INSMOD_SPRWL_KO        ("insmod")
#define RMMOD_SPRWL_KO         ("rmmod")
#define IFCONFIG_WLAN0_UP      ("ifconfig wlan0 up")
#define IFCONFIG_WLAN0_DOWN    ("ifconfig wlan0 down")

#define CMD_LNA_STATUS         ("lna_status")
#define CMD_SET_ENG_MODE       ("set_eng_mode")
#define CMD_RX_GET_OK          ("get_rx_ok")
#define CMD_GET_REG            ("get_reg")
#define CMD_GET_EFUSE          ("get_efuse")
#define CMD_GET_RECONNECT      ("get_reconnect")
#define CMD_GET_RSSI           ("get_rssi")
#define CMD_GET_BEAMF_STATUS   ("get_beamf_status")
#define CMD_GET_RXSTBC_STATUS  ("get_rxstbc_status")

#define IWNPI_EXEC_TMP_FILE            ("/mnt/vendor/iwnpi_exec_data.log")
#define IWNPI_EXEC_BEAMF_STATUS_FILE   ("/mnt/vendor/iwnpi_exec_beamf_status_data.log")
#define IWNPI_EXEC_RXSTBC_STATUS_FILE  ("/mnt/vendor/iwnpi_exec_rxstbc_status_data.log")
#define STR_RET_REG_VALUE              ("ret: reg value:")
#define STR_RET_STATUS_VALUE           ("ret: status:")
#define STR_RET_END                    (":end")
#define STR_RET_RET                    ("ret: ")

#define TMP_BUF_SIZE              (128)
#define CMD_RESULT_BUFFER_LEN     (128)
#define WIFI_EUT_COMMAND_MAX_LEN  (128)

#define for_each_cmd(_cmd)                          \
  for (_cmd = &__start___cmd; _cmd < &__stop___cmd; \
       _cmd = (const struct cmd *)((char *)_cmd + cmd_size))

static void __usage_cmd(const struct cmd *cmd, char *indent, bool full)
{
	const char *start, *lend, *end;

	printf("%s", indent);
	printf("%s", cmd->name);

	if (cmd->args) {
		/* print line by line */
		start = cmd->args;
		end = strchr(start, '\0');
		printf(" ");
		do {
			lend = strchr(start, '\n');
			if (!lend)
				lend = end;
			if (start != cmd->args) {
				printf("\t");
				printf("%s ", cmd->name);
			}
			printf("%.*s\n", (int)(lend - start), start);
			start = lend + 1;
		} while (end != lend);
	} else
		printf("\n");

	if (!full || !cmd->help)
		return;

	/* hack */
	if (strlen(indent))
		indent = "\t\t";
	else
		printf("\n");

	/* print line by line */
	start = cmd->help;
	end = strchr(start, '\0');
	do {
		lend = strchr(start, '\n');
		if (!lend)
			lend = end;
		printf("%s", indent);
		printf("%.*s\n", (int)(lend - start), start);
		start = lend + 1;
	} while (end != lend);

	printf("\n");
}

static void usage_options(void)
{
	printf("Options:\n");
	printf("\t--debug\t\tenable netlink debugging\n");
}

static void usage(int argc, char **argv)
{
	const struct cmd *cmd;
	bool full = argc >= 0;
	const char *sect_filt = NULL;
	const char *cmd_filt = NULL;

	if (argc > 0)
		sect_filt = argv[0];

	if (argc > 1)
		cmd_filt = argv[1];

	printf("Usage:\t%s [wlanX] command\n", argv0);
	usage_options();
	printf("Commands:\n");
	for_each_cmd(cmd) {
		if (sect_filt && strcmp(cmd->name, sect_filt))
			continue;

		if (cmd->handler)
			__usage_cmd(cmd, "\t", full);
	}
}

/********************************************************************
*   name   strcat_safe
*   ---------------------------
*   descrition: wrap strcat func to solve hacker attacks (bug2153526)
*   ----------------------------
*   para        IN/OUT      type                note
*   dest        IN          char *              dest string
*   src         IN          const char *src     src string
*   dest_size   IN          int                 dest space size
*   ----------------------------------------------------
*   return
*   none
*   ------------------
*   other:
*
********************************************************************/
static void strcat_safe(char *dest, const char *src, int dest_size)
{
	int dest_len = strlen(dest);
	int src_len = strlen(src);

	if (dest_len + src_len + 1 > dest_size) {
		ALOGD("ADL %s(), strcat err: src len is too long!\n", __func__);
		return;
	}

	strncat(dest, src, src_len);
}

/********************************************************************
*   name   handle_reply_int_data
*   ---------------------------
*   descrition: handle reply int type data of CMD
*   ----------------------------
*   para        IN/OUT      type                note
*   result_buf  IN          char *              result buffer
*   ----------------------------------------------------
*   return
*   success: the number of parameters to format
*   fail: 0 or -1
*   ------------------
*   other:
*
********************************************************************/
static int handle_reply_int_data(char *result_buf)
{
	int ret = -1;
	FILE *fp = NULL;
	char *str1 = NULL;
	char *str2 = NULL;
	char buf[TMP_BUF_SIZE] = { 0 };
	unsigned char ret_cnt = 0;

	ALOGD("ADL entry %s()", __func__);
	if (NULL == (fp = fopen(IWNPI_EXEC_TMP_FILE, "r+"))) {
		ALOGD("no %s\n", IWNPI_EXEC_TMP_FILE);
		return ret;
	}

	while (!feof(fp)) {
		fgets(buf, TMP_BUF_SIZE, fp);
		str1 = strstr(buf, STR_RET_RET);
		str2 = strstr(buf, STR_RET_END);

		ALOGD("ADL %s(), buf = %s, str1 = %s, str2 = %s", __func__, buf, str1, str2);

		if ((NULL != str1) && (NULL != str2)) {
			/* must be match MARCO STR_RET_RET STR_RET_END */
			ret_cnt = sscanf(buf, "ret: %d: end", &ret);
			ALOGD("ADL %s(), ret_cnt = %d", __func__, ret_cnt);
			if (ret_cnt > 0) {
				ALOGD("ADL %s(), ret = %d, break", __func__, ret);
				break;
			}
		}
		memset(buf, 0, TMP_BUF_SIZE);
	}
	fclose(fp);

	if (result_buf) {
		snprintf(result_buf, CMD_RESULT_BUFFER_LEN, "result: %d", ret);
	}

	ALOGD("ADL leaving %s(), ret = %d, result_buf = %s", __func__, ret, result_buf);
	return ret;
}

/********************************************************************
*   name   handle_reply_beamf_status_data
*   ---------------------------
*   descrition: handle reply data of get_beamf_status CMD
*   ----------------------------
*   para        IN/OUT      type                note
*   result_buf  IN          char *              result buffer
*   ----------------------------------------------------
*   return
*   success: 0
*   fail: -1
*   ------------------
*   other:
*
********************************************************************/
static int handle_reply_beamf_status_data(char *result_buf)
{
	FILE *fp = NULL;
	char *str1 = NULL;
	char *str2 = NULL;
	int ret = -1;
	unsigned long len;
	char buf[TMP_BUF_SIZE] = { 0 };

	ALOGD("ADL entry %s()", __func__);
	if (NULL == (fp = fopen(IWNPI_EXEC_BEAMF_STATUS_FILE, "r+"))) {
		ALOGD("no %s\n", IWNPI_EXEC_BEAMF_STATUS_FILE);
		return ret;
	}

	len = strlen(STR_RET_STATUS_VALUE);
	while (!feof(fp)) {
		fgets(buf, TMP_BUF_SIZE, fp);
		str1 = strstr(buf, STR_RET_STATUS_VALUE);
		str2 = strstr(buf, STR_RET_END);
		ALOGD("ADL %s(), buf = %s, str1 = %s, str2 = %s", __func__, buf, str1, str2);
		if ((NULL != str1) && (NULL != str2)) {
			strncpy(result_buf, buf, CMD_RESULT_BUFFER_LEN);
			ret = 0;
			break;
		}
		memset(buf, 0, TMP_BUF_SIZE);
	}
	fclose(fp);

	ALOGD("ADL leveling %s(), result_buf = %s", __func__, result_buf);
	return ret;
}

/********************************************************************
*   name   handle_reply_rxstbc_status_data
*   ---------------------------
*   descrition: handle reply data of get_rxstbc_status CMD
*   ----------------------------
*   para        IN/OUT      type                note
*   result_buf  IN          char *              result buffer
*   ----------------------------------------------------
*   return
*   success: 0
*   fail: -1
*   ------------------
*   other:
*
********************************************************************/
static int handle_reply_rxstbc_status_data(char *result_buf)
{
	FILE *fp = NULL;
	char *str1 = NULL;
	char *str2 = NULL;
	int ret = -1;
	unsigned long len;
	char buf[TMP_BUF_SIZE] = { 0 };

	ALOGD("ADL entry %s()", __func__);
	if (NULL == (fp = fopen(IWNPI_EXEC_RXSTBC_STATUS_FILE, "r+"))) {
		ALOGD("no %s\n", IWNPI_EXEC_RXSTBC_STATUS_FILE);
		return ret;
	}

	len = strlen(STR_RET_STATUS_VALUE);
	while (!feof(fp)) {
		fgets(buf, TMP_BUF_SIZE, fp);
		str1 = strstr(buf, STR_RET_STATUS_VALUE);
		str2 = strstr(buf, STR_RET_END);
		ALOGD("ADL %s(), buf = %s, str1 = %s, str2 = %s", __func__, buf, str1, str2);
		if ((NULL != str1) && (NULL != str2)) {
			strncpy(result_buf, buf, CMD_RESULT_BUFFER_LEN);
			ret = 0;
			break;
		}
		memset(buf, 0, TMP_BUF_SIZE);
	}
	fclose(fp);

	ALOGD("ADL leveling %s(), result_buf = %s", __func__, result_buf);
	return ret;
}

/********************************************************************
*   name   handle_reply_rx_ok_data
*   ---------------------------
*   descrition: handle reply data of get_rx_ok CMD
*   ----------------------------
*   para        IN/OUT      type                note
*   result_buf  IN          char *              result buffer
*   ----------------------------------------------------
*   return
*   success: 0
*   fail: -1
*   ------------------
*   other:
*
********************************************************************/
static int handle_reply_rx_ok_data(char *result_buf)
{
	FILE *fp = NULL;
	char *str1 = NULL;
	char *str2 = NULL;
	int ret = -1;
	unsigned long len;
	char buf[TMP_BUF_SIZE] = { 0 };

	ALOGD("ADL entry %s()", __func__);
	if (NULL == (fp = fopen(IWNPI_EXEC_TMP_FILE, "r+"))) {
		ALOGD("no %s\n", IWNPI_EXEC_TMP_FILE);
		return ret;
	}

	len = strlen(STR_RET_REG_VALUE);
	while (!feof(fp)) {
		fgets(buf, TMP_BUF_SIZE, fp);
		str1 = strstr(buf, STR_RET_REG_VALUE);
		str2 = strstr(buf, STR_RET_END);

		if ((NULL != str1) && (NULL != str2)) {
			strncpy(result_buf, buf, CMD_RESULT_BUFFER_LEN);
			ret = 0;
			break;
		}
		memset(buf, 0, TMP_BUF_SIZE);
	}
	fclose(fp);

	ALOGD("ADL leveling %s(), result_buf = %s", __func__, result_buf);
	return ret;
}

/********************************************************************
*   name   handle_reply_get_reg_data
*   ---------------------------
*   descrition: handle reply data of get_reg command
*   ----------------------------
*   para        IN/OUT      type                note
*   result_buf  IN          char *              result buffer
*   ----------------------------------------------------
*   return
*   success: 0
*   fail: -1
*   ------------------
*   other:
*
********************************************************************/
static int handle_reply_get_wifi_data(char *result_buf)
{
	FILE *fp = NULL;
	int ret = -1;
	char buf[TMP_BUF_SIZE] = { 0 };

	ALOGD("ADL entry %s()", __func__);
	fp = fopen(IWNPI_EXEC_TMP_FILE, "r+");
	if (NULL == fp) {
		ALOGD("no %s\n", IWNPI_EXEC_TMP_FILE);
		return ret;
	}

	while (!feof(fp)) {
		fgets(buf, TMP_BUF_SIZE, fp);
		strcat(result_buf, buf);
	}

	fclose(fp);
	ALOGD("ADL leaving %s(), result_buf = %s", __func__, result_buf);
	return 0;
}

static int __handle_cmd(struct nlnpi_state *state, int argc, char **argv,
			const struct cmd **cmdout, char *result_buf)
{
	int err;
	char command[WIFI_EUT_COMMAND_MAX_LEN + 1] = { 0x00 };

	ALOGD("ADL entry %s(), line = %d, argc = %d, pid(%d-%d)", __func__, __LINE__, argc,
	      getpid(), getppid());
#if 0
	/* debug, print all paramters */
	{
		unsigned int i = 0;
		while (NULL != argv[i]) {
			ALOGD("ADL %s(), line = %d, argv[%d] = %s", __func__, __LINE__, i, argv[i]);
			i++;
		}
	}
#endif
	if (argc < 1)
		return 1;

	{
		unsigned int i = 0;
		char cmd_str[WIFI_EUT_COMMAND_MAX_LEN + 1] = { "iwnpi " };
		char *module_cmd = cmd_str;

		while (NULL != argv[i]) {
			strcat_safe(cmd_str, argv[i], WIFI_EUT_COMMAND_MAX_LEN);
			if (argv[++i]) {
				/* add a space */
				strcat_safe(cmd_str, " ", WIFI_EUT_COMMAND_MAX_LEN);
			}
		}

		if (NULL != strstr(cmd_str, INSMOD_SPRWL_KO)) {
			module_cmd +=
			    snprintf(module_cmd, CMD_RESULT_BUFFER_LEN, "%s", IFCONFIG_WLAN0_UP);
			*module_cmd = '\0';
		} else if (NULL != strstr(cmd_str, RMMOD_SPRWL_KO)) {
			module_cmd +=
			    snprintf(module_cmd, CMD_RESULT_BUFFER_LEN, "%s", IFCONFIG_WLAN0_DOWN);
			*module_cmd = '\0';
		}
		snprintf(command, WIFI_EUT_COMMAND_MAX_LEN, "%s", cmd_str);

		err = system(command);
		if (WIFEXITED(err) && (WEXITSTATUS(err) == 0)) {
			ALOGD("ADL %s(), called system(%s), successful ret = %d", __func__, command,
			      err);
		} else {
			ALOGD("ADL %s(), called system(%s), child pro exit err = %d, %s", __func__,
			      command, WEXITSTATUS(err), strerror(WEXITSTATUS(err)));
			return -1;
		}

	}

	ALOGD("ADL %s(), command = %s", __func__, command);
	if ((NULL != strstr(command, IFCONFIG_WLAN0_UP))
	    || (NULL != strstr(command, IFCONFIG_WLAN0_DOWN))) {
		if (result_buf)
			snprintf(result_buf, CMD_RESULT_BUFFER_LEN, "result: %d", err);
	} else if ((NULL != strstr(command, CMD_LNA_STATUS))
		   || (NULL != strstr(command, CMD_SET_ENG_MODE))) {
		if (result_buf) {
			ALOGD("ADL %s(), call handle_reply_int_data()", __func__);
			handle_reply_int_data(result_buf);
		}
	} else if (NULL != strstr(command, CMD_GET_BEAMF_STATUS)) {
		if (result_buf) {
			ALOGD("ADL %s(), call CMD_GET_BEAMF_STATUS", __func__);
			handle_reply_beamf_status_data(result_buf);
		}
	} else if (NULL != strstr(command, CMD_GET_RXSTBC_STATUS)) {
		if (result_buf) {
			ALOGD("ADL %s(), call CMD_GET_RXSTBC_STATUS", __func__);
			handle_reply_rxstbc_status_data(result_buf);
		}
	} else if (NULL != strstr(command, CMD_RX_GET_OK)) {
		if (result_buf) {
			ALOGD("ADL %s(), call handle_reply_rx_ok_data()", __func__);
			handle_reply_rx_ok_data(result_buf);
		}
	} else if ((NULL != strstr(command, CMD_GET_REG))
		   || (NULL != strstr(command, CMD_GET_EFUSE))
		   || (NULL != strstr(command, CMD_GET_RECONNECT))
		   || (NULL != strstr(command, CMD_GET_RSSI))) {
		if (result_buf) {
			ALOGD("ADL %s(), call handle_reply_get_wifi_data()", __func__);
			handle_reply_get_wifi_data(result_buf);
		}
	}

	ALOGD("ADL leaving %s(), line = %d, return %d", __func__, __LINE__, err);
	return err;
}

static int nlnpi_init(struct nlnpi_state *state)
{
	int err;

	state->nl_sock = nl_socket_alloc();
	if (!state->nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	nl_socket_set_buffer_size(state->nl_sock, 4096, 4096);

	if (genl_connect(state->nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	/* Android does not support genl_strl_resolve */
	/* state->nlnpi_id = genl_ctrl_resolve(state->nl_sock, "nlnpi"); */
	state->nlnpi_id = NL_GENERAL_NPI_ID;
	/*
	if (state->nlnpi_id < 0) {
		fprintf(stderr, "nlnpi not found.\n");
		err = -ENOENT;
		goto out_handle_destroy;
	}
	*/

	return 0;

out_handle_destroy:
	nl_socket_free(state->nl_sock);
	return err;
}

static void nlnpi_cleanup(struct nlnpi_state *state)
{
	nl_socket_free(state->nl_sock);
}

static int send_back_cmd_result(int client_fd, char *str, int isOK)
{
	char buffer[255];
	int ret = -1;

	if (client_fd < 0) {
		fprintf(stderr, "write %s to invalid fd \n", str);
		return ret;
	}

	memset(buffer, 0, sizeof(buffer));

	if (!str) {
		snprintf(buffer, sizeof(buffer), "%s", (isOK ? OK_STR : FAIL_STR));
	} else {
		snprintf(buffer, sizeof(buffer), "%s %s", (isOK ? OK_STR : FAIL_STR), str);
	}

	ret = write(client_fd, buffer, strlen(buffer) + 1);
	if (ret < 0) {
		fprintf(stderr, "write %s to client_fd:%d fail (error:%s)", buffer,
			client_fd, strerror(errno));
		return ret;
	}

	return 0;
}

/********************************************************************
*   name   iwnpi_runcommand
*   ---------------------------
*   descrition: iwnpi command entry function, wlan0 cmd arg1 arg2 ...
*   ----------------------------
*   para        IN/OUT      type                note
*   client_fd   IN          int                 client file descriptor
*   argc        IN          int                 argument count
*   argv        IN          char **             argument vector
*   ----------------------------------------------------
*   return
*   success: 0
*   fail: -1
*   ------------------
*   other:
*
********************************************************************/
int iwnpi_runcommand(int client_fd, int argc, char **argv)
{
	struct nlnpi_state nlstate;
	int err;
	const struct cmd *cmd = NULL;
	char result_buf[CMD_RESULT_BUFFER_LEN];
	char buf[CMD_RESULT_BUFFER_LEN];

	/* calculate command size including padding */
	cmd_size = abs((long)&__section_set - (long)&__section_get);

	if (argc > 0 && strcmp(*argv, "--debug") == 0) {
		iwnpi_debug = 1;
		argc--;
		argv++;
	}

	/* need to treat "help" command specially so it works w/o nl80211 */
	if (argc == 0 || strcmp(*argv, "help") == 0) {
		usage(argc - 1, argv + 1);
		return 0;
	}

	err = nlnpi_init(&nlstate);
	if (err)
		return 1;

	memset(result_buf, 0, sizeof(result_buf));
	memset(buf, 0, sizeof(buf));
	if (strncmp(*argv, "wlan", 4) == 0 && argc > 1) {
		err = __handle_cmd(&nlstate, argc, argv, &cmd, result_buf);
	} else {
		fprintf(stderr, "wireless dev start with wlan\n");
		err = 1;
	}

	if (err == 1) {
		send_back_cmd_result(client_fd, "invalid cmd or cmd format", 0);
	} else if (err < 0) {
		snprintf(buf, sizeof(buf), "command failed return value: %d", err);
		send_back_cmd_result(client_fd, buf, 0);
	} else {
		if (strlen(result_buf))
			snprintf(buf, sizeof(buf), "%s", result_buf);
		else
			snprintf(buf, sizeof(buf), "return value: %d", err);

		send_back_cmd_result(client_fd, buf, 1);
	}

	nlnpi_cleanup(&nlstate);

	return err;
}
