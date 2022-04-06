/******************************************************************************
 *
 *  Filename:      wcn_vendor.c
 *
 *  Description:   unisoc wcn vendor specific library implementation
 *
 ******************************************************************************/

#define LOG_TAG "wcn_vendor"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <string.h>
#include "wcn_vendor.h"

#if (WCNVND_DBG == TRUE)
#define WCNVNDDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define WCNVNDDBG(param, ...) {}
#endif

#define WCN_AT_PROC_NODE "/proc/mdbg/at_cmd"
#define SW_SYSFS_PATH "/sys/class/misc/wcn/devices/sw_ver"
#define HW_SYSFS_PATH "/sys/class/misc/wcn/devices/hw_ver"
#define LOGLEVEL_SYSFS_PATH "/sys/class/misc/wcn/devices/loglevel"
#define ARMLOG_SYSFS_PATH "/sys/class/misc/wcn/devices/armlog_status"
#define ATCMD_SYSFS_PATH "/sys/class/misc/wcn/devices/atcmd"
#define RESET_DUMP_SYSFS_PATH "/sys/class/misc/wcn/devices/reset_dump"

#define LOG_ON 1
#define LOG_OFF 0

static int init(void)
{
	int fd = -1;
	int ret = -1;
	char buf[16];

	fd = open(WCN_AT_PROC_NODE, O_WRONLY);
	if (fd < 0) {
		ALOGE("open(%s) for write failed: %s (%d)",
		      WCN_AT_PROC_NODE, strerror(errno), errno);
		return ret;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "startwcn");

	if (write(fd, &buf, 8) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      WCN_AT_PROC_NODE, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

/** Requested operations */
static int get_sw_version(char *buf, int len)
{
	int fd = -1;
	int ret = -1;

	fd = open(SW_SYSFS_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE("open(%s) for read failed: %s (%d)",
		      SW_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	len = read(fd, buf, len);
	if (len <= 0) {
		ALOGE("read(%s) for read failed: %s (%d)",
		      SW_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int get_hw_version(char *buf, int len)
{
	int fd = -1;
	int ret = -1;

	fd = open(HW_SYSFS_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE("open(%s) for read failed: %s (%d)",
		      HW_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	len = read(fd, buf, len);
	if (len <= 0) {
		ALOGE("read(%s) for read failed: %s (%d)",
		      HW_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int get_loglevel(char *buf, int len)
{
	int fd = -1;
	int ret = -1;

	fd = open(LOGLEVEL_SYSFS_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	len = read(fd, buf, len);
	if (len <= 0) {
		ALOGE("read(%s) for read failed: %s (%d)",
		      LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int set_loglevel(char *buf, int len)
{
	int fd = -1;
	int ret = -1;

	fd = open(LOGLEVEL_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE("open(%s) for read failed: %s (%d)",
		      LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}
	ALOGI("loglevel set buf =%s\n", buf);
	if (write(fd, buf, 1) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int enable_armlog(void)
{
	int fd = -1;
	int ret = -1;
	char buf = '1';

	fd = open(ARMLOG_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	if (write(fd, &buf, 1) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int disable_armlog(void)
{
	int fd = -1;
	int ret = -1;
	char buf = '0';

	fd = open(ARMLOG_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	if (write(fd, &buf, 1) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int get_armlog_status(void)
{
	int fd = -1;
	int ret = -1;
	char buf = '0';
	int len;

	fd = open(ARMLOG_SYSFS_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	len = read(fd, &buf, 1);
	if (len <= 0) {
		ALOGE("read(%s) for read failed: %s (%d)",
		      ARMLOG_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	if (ret < 0)
		return ret;

	if (buf == '1')
		return LOG_ON;
	if (buf == '0')
		return LOG_OFF;

	return ret;
}

static int send_at_cmd(char *buf, int len)
{
	int fd = -1;
	int ret = -1;

	fd = open(ATCMD_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      ATCMD_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	if (write(fd, buf, len) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      ATCMD_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int manual_assert(void)
{
	int ret = -1;
	char buf[] = "at+spatassert=1\r";

	ret = send_at_cmd(buf, sizeof(buf));

	return ret;
}

static int manual_dumpmem(void)
{
	int fd = -1;
	int ret = -1;
	char buf[] = "manual_dump";

	fd = open(RESET_DUMP_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	if (write(fd, buf, sizeof(buf)) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int reset_dump_opt(int reset_type)
{
	int fd = -1;
	int ret = -1;
	char buf[] = "reset_dump";  //reset default type is reset and dump

	if (reset_type == WCND_DUMP) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "dump");
	} else if (reset_type == WCND_RESET) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "reset");
	}

	fd = open(RESET_DUMP_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
		return ret;
	}

	if (write(fd, buf, sizeof(buf)) < 0) {
		ALOGE("write(%s) failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int get_reset_status(void)
{
	int fd = -1;
	int len;
	char buf[12];

	fd = open(RESET_DUMP_SYSFS_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE(" open(%s) for read failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
		return -1;
	}

	len = read(fd, &buf, (sizeof(buf) - 1));
	if (len <= 0) {
		ALOGE("read(%s) for read failed: %s (%d)",
		      RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
		close(fd);
		return -1;
	}

	close(fd);
	buf[len] = '\0';

	if (!strcmp(buf, "dump\n"))
		return WCND_DUMP;
	else if (!strcmp(buf, "reset\n"))
		return WCND_RESET;
	else if (!strcmp(buf, "reset_dump\n"))
		return WCND_RESET_DUMP;
	else
		return -1;
}

/** Closes the interface */
static int cleanup( void )
{
	int fd = -1;
	int ret = -1;
	char buf[16];

	fd = open(WCN_AT_PROC_NODE, O_WRONLY);
	if (fd < 0) {
		ALOGE("int : open(%s) for write failed: %s (%d)",
		      WCN_AT_PROC_NODE, strerror(errno), errno);
		return ret;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "stopwcn");

	if (write(fd, &buf, 7) < 0) {
		ALOGE("int : write(%s) failed: %s (%d)",
		      WCN_AT_PROC_NODE, strerror(errno), errno);
	} else {
		ret = 0;
	}

	if (fd >= 0)
		close(fd);

	return ret;
}

static int wcn_cmd_opt(char *argv[], char *result, int len, int *error_type)
{
	int ret = -1;
	char buffer[WCND_SOCKET_CMD_REPLY_LENGTH];
	char buf = argv[0][strlen(argv[0]) - 1];

	if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_VERSION)) {
		ret = get_sw_version(result, len);
	} else if (strcasestr(argv[0], WCND_SOCKET_CMD_SET_CP2_LOGLEVEL)) {
		ret = set_loglevel(&buf, len);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_GET_CP2_LOGLEVEL)) {
		ret = get_loglevel(result, len);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_ENABLE_CP2_LOG)) {
		ret = enable_armlog();
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_DISABLE_CP2_LOG)) {
		ret = disable_armlog();
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_LOG_STATUS)) {
		ret = get_armlog_status();
		if (ret >= 0) {
			snprintf(result, WCND_SOCKET_CMD_REPLY_LENGTH, "%s%d",
				 WCND_SOCKET_CMD_CP2_LOG_STATUS_RSP, ret);
		}
	} else if (strcasestr(argv[0], WCND_SOCKET_CMD_CP2_ASSERT)) {
		ret = manual_assert();
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_DUMP)) {
		ret = manual_dumpmem();
	} else if (!strcmp(argv[0], WCND_SOKCET_CMD_CP2_DUMP_ENABLE)) {
		ret = 0;
		ALOGE("%s, cmd %s should support, not just return success",
		      __func__, argv[0]);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_DUMP_DISABLE)) {
		ret = 0;
		ALOGE("%s, cmd %s should support, not just return success",
		      __func__, argv[0]);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_RESET_STATUS)) {
		ret = get_reset_status();
		if (ret >= 0) {
			snprintf(result, WCND_SOCKET_CMD_REPLY_LENGTH, "%s%d",
				 WCND_SOCKET_CMD_CP2_RESET_STATUS_RSP, ret);
		}
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_RESET_DISABLE)) {
		ret = reset_dump_opt(WCND_DUMP);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_RESET_ENABLE)) {
		ret = reset_dump_opt(WCND_RESET);
	} else if (!strcmp(argv[0], WCND_SOCKET_CMD_CP2_RESET_DUMP)) {
		ret = reset_dump_opt(WCND_RESET_DUMP);
	} else if (strstr(argv[0], WCND_SOCKET_CMD_AT_PREFIX)) {
		ALOGD("WcnVendorApi send_at_cmd");
		snprintf(buffer, sizeof(buffer), "%s", argv[0]);
		len = strlen(argv[0]);
		// WCN at cmd must end with '\r'
		if (buffer[len - 1] != '\r') {
			if (len < (WCND_SOCKET_CMD_REPLY_LENGTH - 1)) {
				buffer[len] = '\r';
				len++;
				ret = send_at_cmd(buffer, len);
			} else {
				*error_type = WCND_SOCKET_CMD_ERROR_QUOTE;
			}
		} else {
			ret = send_at_cmd(buffer, len);
		}
	}
	return ret;
}


// Entry point of DLib
const wcn_vendor_interface_t WCN_VENDOR_LIB_INTERFACE = {
	sizeof(wcn_vendor_interface_t),
	wcn_cmd_opt
};
