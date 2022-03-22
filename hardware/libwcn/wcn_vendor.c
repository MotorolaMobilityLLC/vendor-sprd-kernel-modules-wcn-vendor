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
    if (fd < 0)
     {
        ALOGE("open(%s) for write failed: %s (%d)",
                   WCN_AT_PROC_NODE, strerror(errno), errno);
                return ret;
    }

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "startwcn");

    if (write(fd, &buf, 8) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        WCN_AT_PROC_NODE, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}


/** Requested operations */
static int get_sw_version(char *buf, int *len)
{
    int fd = -1;
    int ret = -1;

    fd = open(SW_SYSFS_PATH, O_RDONLY);
    if (fd < 0)
     {
        ALOGE("open(%s) for read failed: %s (%d)",
                   SW_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    *len = read(fd, buf, *len);
    if (*len <= 0)
     {
        ALOGE("read(%s) for read failed: %s (%d)",
                   SW_SYSFS_PATH, strerror(errno), errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

static int get_hw_version(char *buf, int *len)
{
    int fd = -1;
    int ret = -1;

    fd = open(HW_SYSFS_PATH, O_RDONLY);
    if (fd < 0)
     {
        ALOGE("open(%s) for read failed: %s (%d)",
                   HW_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    *len = read(fd, buf, *len);
    if (*len <= 0)
     {
        ALOGE("read(%s) for read failed: %s (%d)",
                   HW_SYSFS_PATH, strerror(errno), errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

static int get_loglevel(char *buf, int *len)
{
    int fd = -1;
    int ret = -1;

    fd = open(LOGLEVEL_SYSFS_PATH, O_RDONLY);
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    *len = read(fd, buf, *len);
    if (*len <= 0)
     {
        ALOGE("read(%s) for read failed: %s (%d)",
                   LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

static int set_loglevel(char *buf, int len)
{
    int fd = -1;
    int ret = -1;

    fd = open(LOGLEVEL_SYSFS_PATH, O_WRONLY);
    if (fd < 0)
     {
        ALOGE("open(%s) for read failed: %s (%d)",
                   LOGLEVEL_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }
    ALOGI("loglevel set buf =%s\n", buf);
    if (write(fd, buf, 1) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        LOGLEVEL_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0;

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
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   ARMLOG_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    if (write(fd, &buf, 1) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        ARMLOG_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0;

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
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   ARMLOG_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    if (write(fd, &buf, 1) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        ARMLOG_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0;

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
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   ARMLOG_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    len = read(fd, &buf, 1);
    if (len <= 0)
     {
        ALOGE("read(%s) for read failed: %s (%d)",
                   ARMLOG_SYSFS_PATH, strerror(errno), errno);
    }
    else
        ret = 0;

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
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   ATCMD_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    if (write(fd, buf, len) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        ATCMD_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0;

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
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    if (write(fd, buf, sizeof(buf)) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        RESET_DUMP_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

static int reset(bool is_reset)
{
    int fd = -1;
    int ret = -1;
    char buf[] = "reset";

    if (!is_reset) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "dump");
    }

    fd = open(RESET_DUMP_SYSFS_PATH, O_WRONLY);
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
                return ret;
    }

    if (write(fd, buf, sizeof(buf)) < 0)
    {
        ALOGE("write(%s) failed: %s (%d)",
                        RESET_DUMP_SYSFS_PATH, strerror(errno),errno);
    }
    else
        ret = 0 ;

    if (fd >= 0)
        close(fd);

    return ret;
}

static int get_reset_status(void)
{
    int fd = -1;
    int len;
    char buf[6] = {0};

    fd = open(RESET_DUMP_SYSFS_PATH, O_RDONLY);
    if (fd < 0)
     {
        ALOGE(" open(%s) for read failed: %s (%d)",
                   RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
                return -1;
    }

    len = read(fd, &buf, 5);
    if (len <= 0)
     {
        ALOGE("read(%s) for read failed: %s (%d)",
                   RESET_DUMP_SYSFS_PATH, strerror(errno), errno);
        close(fd);
        return -1;
    }

    close(fd);

    if (strncmp(buf, "reset", 5) == 0)
        return 1;

    if (strncmp(buf, "dump", 4) == 0)
         return 0;

    return -1;
}

/** Closes the interface */
static int cleanup( void )
{
    int fd = -1;
    int ret = -1;
    char buf[16];

    fd = open(WCN_AT_PROC_NODE, O_WRONLY);
    if (fd < 0)
     {
        ALOGE("int : open(%s) for write failed: %s (%d)",
                   WCN_AT_PROC_NODE, strerror(errno), errno);
                return ret;
    }

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "stopwcn");

    if (write(fd, &buf, 7) < 0)
    {
        ALOGE("int : write(%s) failed: %s (%d)",
                        WCN_AT_PROC_NODE, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}


// Entry point of DLib
const wcn_vendor_interface_t WCN_VENDOR_LIB_INTERFACE = {
    sizeof(wcn_vendor_interface_t),
    init,
    get_sw_version,
    get_hw_version,
    get_loglevel,
    set_loglevel,
    enable_armlog,
    disable_armlog,
    get_armlog_status,
    manual_assert,
    manual_dumpmem,
    reset,
    get_reset_status,
    send_at_cmd,
    cleanup
};
