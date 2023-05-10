/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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

#define LOG_TAG "bluetooth_chr"

//#include <utils/Log.h>
#include <log/log.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include "bt_vendor_lib.h"
//#include "btsnoop_sprd_for_raw.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <cutils/sockets.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/eventfd.h>

#include <cutils/properties.h>

#include "string.h"
#include "bluetooth_chr.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef SNOOP_DBG
    #define SNOOP_DBG TRUE
#endif

#if (SNOOP_DBG == TRUE)
    #define SNOOPD(param, ...) ALOGD("%s " param, __FUNCTION__, ## __VA_ARGS__)
#else
    #define SNOOPD(param, ...) {}
#endif

#ifdef SNOOPD_V
    #define SNOOPV(param, ...) ALOGD("%s " param, __FUNCTION__, ## __VA_ARGS__)
#else
    #define SNOOPV(param, ...) {}
#endif

#define SNOOPE(param, ...) ALOGE("%s " param, __FUNCTION__, ## __VA_ARGS__)


/* macro for parsing config file */
#define BTE_STACK_CONF_FILE "/data/misc/bluedroid/bt_stack.conf"
#define CONF_MAX_LINE_LEN 255
#define CONF_COMMENT '#'
#define CONF_DELIMITERS " =\n\r\t"
#define CONF_VALUES_DELIMITERS "=\n\r\t#"

#define EFD_SEMAPHORE (1 << 0)
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

/* macro for socket */
#define LOG_SOCKET_FOR_HCILOG "bt_snoop_slog_socket_for_hcilog"
#define LOG_SOCKET_CTL_FOR_HCILOG "bt_snoop_slog_socket_CTL_for_hcilog"


#define LOG_TURN_ON "ON"
#define LOG_TURN_OFF "OFF"
#define CONTROL_ACK "ACK"
#define CONTROL_NACK "NACK"

#define HCI_EDR3_DH5_PACKET_SIZE    1021
#define BTSNOOP_HEAD_SIZE  25
#define HCI_PIIL_SVAE_BUF_SIZE 5
#define SLOG_STREAM_OUTPUT_BUFFER_SZ      ((HCI_EDR3_DH5_PACKET_SIZE + BTSNOOP_HEAD_SIZE) * 5)
#define HCI_POLL_SIZE (HCI_EDR3_DH5_PACKET_SIZE + BTSNOOP_HEAD_SIZE + HCI_PIIL_SVAE_BUF_SIZE)
#define CTRL_CHAN_RETRY_COUNT 3
#define MAX_EPOLL_EVENTS 3
#define INVALID_FD -1
#define CASE_RETURN_STR(const) case const: return #const;
#define UNUSED(x) (void)(x)

/*  Camella new requirements  */
/**************Static Variables*****************/
# define MSG_NOSIGNAL 0x4000
#define WRITE_POLL_MS 20
#define SOCK_SEND_TIMEOUT_MS 2000 /* Timeout for sending */
#define MSG_DONTWAIT  0x40
#define EAGAIN 11
#define WCNCHR_BLUETOOTH_SOCKET "wcnchr_bluetooth"
#define TRUE  1
#define FALSE 0
#define OSI_NO_INTR(fn) \
   do {                  \
} while ((fn) == -1 && errno == EINTR)
//#define sem_init(sem, sem_attr1, sem_init_value) (int)((*sem = CreateSemaphore(NULL,0,32768,NULL))==NULL)

#ifdef FUNCTION_NEED_RELEASE
#define BIT_0 0x1
#define BIT_1 0x2
#define BIT_2 0x4
#define BIT_3 0x8
#define BIT_4 0x10
#define BIT_5 0x20
#define BIT_6 0x40
#define BIT_7 0x80
#endif
/**********************************************/

/******************param*********************/
extern uint8_t bt_chr_event_bitmap_30001;
extern int chr_skt_fd;
char chr_ind_30001[512] = {0};
#ifdef FUNCTION_NEED_RELEASE
static uint8_t bt_chr_event_bitmap_2 = 0;
static uint8_t bt_chr_event_bitmap_3 = 0;
static uint8_t bt_chr_event_bitmap_4 = 0;
#endif

#define TRUE 1
#define FALSE 0
uint8_t thread_isrunning;
pthread_t thread_id;

/**********************************************/

/**********************************************/
typedef enum
{
    CMD = 1,
    ACL,
    SCO,
    EVE
} packet_type_t;

/*********BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS*********/
/*function: select_num_from_string*/
/*usage: select num from event_id*/
int select_num_from_string(char *num_in_str)
{
    int number = 0;

    for(int i=0;num_in_str[i]!='\0';i++)
        if((num_in_str[i]<='9') && (num_in_str[i]>='0'))/*is numeric character or not*/
            number=number*10+(num_in_str[i])-'0';/*numeric character set to number*/
    return number;
}

//Set the CHR command format，change string to params
//#define WCN_CHR_SOCKET_CMD_SET_EVENT "wcn_chr_set_event,module=bt/gnss,length=xx(bytes),value1,value2,value3,......"
char **explode(char sep, const char *str, int *size)
{
    int count = 0, i;
    for(i = 0; i < strlen(str); i++)
    {
        if (str[i] == sep)
        {
            count ++;
        }
    }

    char **ret = calloc(++count, sizeof(char *));

    int lastindex = -1;
    int j = 0;

    for(i = 0; i < strlen(str); i++)
    {
        if (str[i] == sep)
        {
            ret[j] = calloc(i - lastindex, sizeof(char));
            memcpy(ret[j], str + lastindex + 1, i - lastindex - 1);
            j++;
            lastindex = i;
        }
    }
        //handle the last string
    if (lastindex <= strlen(str) - 1)
    {
        ret[j] = calloc(strlen(str) - lastindex, sizeof(char));
        memcpy(ret[j], str + lastindex + 1, strlen(str) - 1 - lastindex);
        j++;
    }

    *size = j;

    return ret;

}

static int skt_read(int fd, void* p, size_t len)
{
    ssize_t read;

    //ts_log("skt_read recv", len, NULL);
    OSI_NO_INTR(read = recv(fd, p, len, MSG_NOSIGNAL));
    // ERROR errno first to void
    //if (read == -1) ERROR("read failed with errno=%d\n", errno);

    return (int)read;
}

/*

static int skt_write(int fd, const void* p, size_t len)
{
    ssize_t sent;
    SNOOPE("skt_write");
    //ts_log("skt_write", len, NULL);

    if (WRITE_POLL_MS == 0) {
        // do not poll, use blocking send
        OSI_NO_INTR(sent = send(fd, p, len, MSG_NOSIGNAL));
        //if (sent == -1) ERROR("write failed with error(%s)", strerror(errno)); 
        return (int)sent;
    }

    // use non-blocking send, poll
    int ms_timeout = SOCK_SEND_TIMEOUT_MS;
    size_t count = 0;
    while (count < len) {
        OSI_NO_INTR(sent = send(fd, p, len - count, MSG_NOSIGNAL | MSG_DONTWAIT));
        if (sent == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                //ERROR("write failed with error(%s)", strerror(errno));
                return -1;
            }
            if (ms_timeout >= WRITE_POLL_MS) {
                usleep(WRITE_POLL_MS * 1000);
                ms_timeout -= WRITE_POLL_MS;
                continue;
            }
            //WARN("write timeout exceeded, sent %zu bytes", count);
          return -1;
        }
        count += sent;
        p = (const uint8_t*)p + sent;
    }
    return (int)count;
}
*/

void recv_btchrmsg(uint8_t *data)
{
    uint8_t *recv_chr_data;
    recv_chr_data = data;

    //event_id:abnormal event + its first event
    if (recv_chr_data[3] == 0x0 && recv_chr_data[4] == 0x0)
    {
        bt_chr_event_bitmap_30001++;
        ALOGD("bt_chr_event_bitmap_30001:%d",bt_chr_event_bitmap_30001);
        sprintf(chr_ind_30001,"0x%x,0x%x", recv_chr_data[5], recv_chr_data[6]);
        ALOGD("bt_chr_event_bitmap_30001: %d, sprint chr_ind_30001:%s",bt_chr_event_bitmap_30001, chr_ind_30001);
    }
    return;
}


static void bluetooth_chr_thread(void* param)
{
    //int chr_skt_fd;
    int bytes = 0;
    char buffer[1024] = {0};
    char **read_msg = NULL;
    uint32_t skt_read_value = 0;
    uint32_t skt_write_value = 0;
    struct sockaddr_in bt_addr = {0};
    bt_chr_ind_msg_struct msg = {0};
    char temp_ind_event[1024] = {0};

    msg.ref_count = 0;
    msg.version = 1;

    UNUSED(param);

    //create socket
    chr_skt_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(chr_skt_fd < 0){
        SNOOPE("chr_skt_fd create fail errno = %d, err str:%s:", errno, strerror(errno));
        goto failed;
    }else{
        SNOOPE("chr_skt_fd = %d", chr_skt_fd);
    }

    //use AD_INET to connect BSP
    bt_addr.sin_family = AF_INET;
    bt_addr.sin_port = htons(4757);
    inet_pton(AF_INET, "127.0.0.1", &bt_addr.sin_addr);
    if(!connect(chr_skt_fd, (struct sockaddr *)&bt_addr, sizeof(bt_addr))){
        SNOOPE("bt connect wcn_chr server succeed");
    }
    else{
        SNOOPE("bt connect wcn_chr server fail errno=%d, err str:%s", errno, strerror(errno));
        goto failed;
    }

    //connect success wait BSP indicate wcn_chr_set_event
    usleep(30*1000);
    SNOOPE("usleep 30ms");

    //BSP set chr enable or disable
    //wcn_chr_set_event,module=GNSS,event_id=0x30001,set=1,maxcount=0x100,tlimit=0x1
    SNOOPE("add memset");
    memset(buffer, 0 ,sizeof(buffer));
    SNOOPE("enter read");
    //chr will jam because function read will keep waiting
    skt_read_value = skt_read(chr_skt_fd, buffer, sizeof(buffer));
    SNOOPE("chr_skt_fd = %d, skt_read_value = %d, buffer = %s",chr_skt_fd,skt_read_value,buffer);

    //if skt_read_value = -1  :disconnect                   ----close(chr_skt_fd)
    //if skt_read_value = 0   :read time out                ----close(chr_skt_fd)
    if(skt_read_value <= 0){
        SNOOPE("bt read from wcn_chr server failed");
        goto failed;
    }

    //if skt_read_value >= 0  :read something from socket   ----enable or disable chr
    bytes = sizeof(buffer);
    read_msg = explode(',', buffer, &bytes);
    //wcn_chr_set_event,module=BT,event_id=0x30001,set=1,maxcount=0x100,tlimit=0x1
    //wcn_chr_set_event,module=BT,event_id=xx,set=0

    //BSP send cmd to disable bt chr
    if (select_num_from_string(read_msg[3]) == 0)
    {
        SNOOPE("bt chr disabled");
        if(read_msg[0])
        {
            free(read_msg[0]);
        }
        read_msg[0] = NULL;

        if(read_msg[1])
        {
            free(read_msg[1]);
        }
        read_msg[1] = NULL;

        if(read_msg[2])
        {
            free(read_msg[2]);
        }
        read_msg[2] = NULL;

        if(read_msg[3])
        {
            free(read_msg[3]);
        }
        read_msg[3] = NULL;

        if(read_msg)
        {
            free(read_msg);
        }
        read_msg = NULL;
        goto disabled_chr;
    }

    //BSP send cmd to enable bt chr
    if (select_num_from_string(read_msg[3]) == 1)
    {
        SNOOPE("bt chr enabled");
        do {
            //the reported error from whether will be indicated to BSP decided on its event_id
            //wcn_chr_ind_event,modules=bt/gnss/wifi/bsp,ref_count=XX,event_id=XX,version=XX,event_content_len=XX,chr_info=XX;
            if (msg.ref_count < bt_chr_event_bitmap_30001)
            {
                ALOGD("recv msg from firmware msg.ref_count: %d, bt_chr_event_bitmap_30001:%d, chr_ind_30001;%s",msg.ref_count, bt_chr_event_bitmap_30001,chr_ind_30001);
                msg.ref_count = bt_chr_event_bitmap_30001;
                msg.event_id = 0x23001;
                msg.event_content = chr_ind_30001;
                msg.event_content_len = strlen(chr_ind_30001);

                sprintf(temp_ind_event,"wcn_chr_ind_event,module=BT,ref_count=%d,event_id=0x%x,version=%d,event_content_len=%d,char_info=%s",
                                                   msg.ref_count, msg.event_id, msg.version, msg.event_content_len, msg.event_content);
                ALOGD("temp_ind_event: %s",temp_ind_event);
                skt_write_value = write(chr_skt_fd, temp_ind_event, strlen(temp_ind_event));
                if(skt_write_value < 0){
                    SNOOPE("bt write to wcn_chr server failed");
                    if(read_msg[0])
                    {
                        free(read_msg[0]);
                    }
                    read_msg[0] = NULL;

                    if(read_msg[1])
                    {
                        free(read_msg[1]);
                    }
                    read_msg[1] = NULL;

                    if(read_msg[2])
                    {
                        free(read_msg[2]);
                    }
                    read_msg[2] = NULL;

                    if(read_msg[3])
                    {
                        free(read_msg[3]);
                    }
                    read_msg[3] = NULL;

                    if(read_msg[4])
                    {
                        free(read_msg[4]);
                    }
                    read_msg[4] = NULL;

                    if(read_msg[5])
                    {
                        free(read_msg[5]);
                    }
                    read_msg[5] = NULL;

                    if(read_msg)
                    {
                        free(read_msg);
                    }
                    read_msg = NULL;
                    goto failed;
                }
            }
        //power save
        sleep(5);
        //SNOOPE("thread_isrunning = %d",thread_isrunning);
        } while (thread_isrunning);
    }
    //release chr_skt_fd
    close(chr_skt_fd);
    return;
failed:
    close(chr_skt_fd);
    SNOOPE("failed close chr_skt_fd");
    return;
disabled_chr:
    close(chr_skt_fd);
    SNOOPE("disabled_chr close chr_skt_fd");
    return;
}


void bluetooth_chr_init(void)
{
    int ret = -1;
    //pthread_t thread_id;
    thread_isrunning = TRUE;

    ret = pthread_create(&thread_id, NULL,(void*)bluetooth_chr_thread, NULL);
    if(ret){
        thread_isrunning = FALSE;
        SNOOPE("bluetooth_chr_init pthread_create failed");
        return;
    }
    //pthread_join(thread_id, NULL);
    //return;
}


void bluetooth_chr_cleanup(void)
{
    shutdown(chr_skt_fd, SHUT_RDWR);
    SNOOPE("bluetooth_chr_cleanup shutdown");

    usleep(1*1000);

    thread_isrunning = FALSE;

    SNOOPE("thread_isrunning=%d",thread_isrunning);

    pthread_join(thread_id, NULL);

    SNOOPE("bluetooth_chr_cleanup pthread_join");

}
