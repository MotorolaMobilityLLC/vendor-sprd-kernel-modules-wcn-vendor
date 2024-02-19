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

#define LOG_TAG "bt_chr"

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
#include <signal.h>
#include <inttypes.h>
#include "bt_vendor_lib.h"
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
extern uint8_t bt_chr_event_bitmap_30002;
extern int chr_skt_fd;
extern char **read_msg;
char chr_ind_30002[512] = {0};
#ifdef FUNCTION_NEED_RELEASE
static uint8_t bt_chr_event_bitmap_2 = 0;
static uint8_t bt_chr_event_bitmap_3 = 0;
static uint8_t bt_chr_event_bitmap_4 = 0;
#endif

#define TRUE 1
#define FALSE 0
#define WCN_CHR_SOCKET_CMD_ENABLE       "wcn_chr_enable"
#define WCN_CHR_SOCKET_CMD_DISABLE      "wcn_chr_disable"
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
int select_num_from_string(char *num_in_str) {
    int number = 0;

    for(int i=0;num_in_str[i]!='\0';i++)
	 if((num_in_str[i]<='9') && (num_in_str[i]>='0'))/*determine if it is num character*/
             number=number*10+(num_in_str[i])-'0';/*num character change to num*/
    return number;
}

//Set the CHR command format，change string to params
//#define WCN_CHR_SOCKET_CMD_SET_EVENT "wcn_chr_set_event,module=bt/gnss,length=xx(bytes),value1,value2,value3,......"
char **explode(char sep, const char *str, int *size) {
    int count = 0, i;
    for(i = 0; i < strlen(str); i++)
    {
        if (str[i] == sep) {
            count ++;
        }
    }

    char **ret = calloc(++count, sizeof(char *));

    int lastindex = -1;
    int j = 0;

    for(i = 0; i < strlen(str); i++) {
        if (str[i] == sep) {
            ret[j] = calloc(i - lastindex, sizeof(char));
            memcpy(ret[j], str + lastindex + 1, i - lastindex - 1);
            j++;
            lastindex = i;
        }
    }

    //handle the last string
    if (lastindex <= strlen(str) - 1) {
        ret[j] = calloc(strlen(str) - lastindex, sizeof(char));
        memcpy(ret[j], str + lastindex + 1, strlen(str) - 1 - lastindex);
        j++;
    }

    *size = j;

    return ret;
}

static int skt_read(int fd, void* p, size_t len) {
    ssize_t read;

    //ts_log("skt_read recv", len, NULL);
    OSI_NO_INTR(read = recv(fd, p, len, MSG_NOSIGNAL));
    // ERROR errno first to void
    //if (read == -1) ERROR("read failed with errno=%d\n", errno);

    return (int)read;
}

void recv_btchrmsg(uint8_t *data) {
    uint8_t *recv_chr_data;
    recv_chr_data = data;

    //event_id:abnormal event + its first event
    if (recv_chr_data[3] == 0x0 && recv_chr_data[4] == 0x0) {
	 bt_chr_event_bitmap_30002++;
         SNOOPD("bt_chr_event_bitmap_30002:%d",bt_chr_event_bitmap_30002);
         sprintf(chr_ind_30002,"0x%x,0x%x", recv_chr_data[5], recv_chr_data[6]);
         SNOOPD("bt_chr_event_bitmap_30002: %d, sprint chr_ind_30002:%s",
                 bt_chr_event_bitmap_30002, chr_ind_30002);
    }
}

void bluetooth_chr_clear_chr_ind() {    
    int i;

    if (read_msg) {
        if (select_num_from_string(read_msg[3]) == 0) {
            for (i = 0; i <= 3; i++) {
                if (read_msg[i]) {
                    free(read_msg[i]);
                }
                read_msg[i] = NULL;
            }
            if (read_msg) {
                free(read_msg);
            }
            read_msg = NULL;
        } else {
            for (i = 0; i <= 5; i++) {
                if (read_msg[i]) {
                    free(read_msg[i]);
                }
                read_msg[i] = NULL;
            }
            if (read_msg) {
                free(read_msg);
            }
            read_msg = NULL;
        }
    }
 }

int bluetooth_connect_chr() {
    struct sockaddr_in bt_addr = {0};

    //create socket
    chr_skt_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (chr_skt_fd < 0) {
        SNOOPE("chr_skt_fd create fail errno = %d, err str:%s:",
                errno, strerror(errno));
        return FALSE;
    } else {
        SNOOPD("chr_skt_fd = %d", chr_skt_fd);
    }

    //use AD_INET to connect BSP
    bt_addr.sin_family = AF_INET;
    bt_addr.sin_port = htons(4757);
    inet_pton(AF_INET, "127.0.0.1", &bt_addr.sin_addr);
    if(!connect(chr_skt_fd, (struct sockaddr *)&bt_addr, sizeof(bt_addr))) {
        SNOOPD("bt connect wcn_chr server succeed");
        return TRUE;
    } else {
        SNOOPE("bt connect wcn_chr server fail errno=%d, err str:%s",
                errno, strerror(errno));
        return FALSE;
    }
}

void bluetooth_release_skt(int chr_skt_fd) {
    if (chr_skt_fd > 0) {
        close(chr_skt_fd);
        chr_skt_fd = -1;
    }
}

static void bluetooth_chr_thread_exit() {
    SNOOPD("exit");
    bluetooth_release_skt(chr_skt_fd);
    bluetooth_chr_clear_chr_ind();

    pthread_exit(0);
}

static void bluetooth_chr_thread(void* param) {
    int bytes = 0;
    char buffer[1024] = {0};
    uint32_t skt_read_value = 0;
    uint32_t skt_write_value = 0;
    bt_chr_ind_msg_struct msg = {0};
    char temp_ind_event[1024] = {0};
    fd_set fds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    //Signal action
    struct sigaction actions;
    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = bluetooth_chr_thread_exit;
    sigaction(SIGUSR2,&actions,NULL);

    msg.ref_count = 0;
    msg.version = 1;

    UNUSED(param);

    connect_p:
    //SNOOPD("connect");

    //create socket
    if (!bluetooth_connect_chr()) {
        goto exit_p;
    }

    SNOOPD("usleep 30ms");
    usleep(30*1000);

    memset(buffer, 0 ,sizeof(buffer));
    //chr will jam because function read will keep waiting
    SNOOPD("start read");
    skt_read_value = skt_read(chr_skt_fd, buffer, sizeof(buffer));
    /*chr ind*/
    //wcn_chr_enable
    //wcn_chr_disable
    //wcn_chr_set_event,module=BT,event_id=0x30002,set=1,maxcount=0x100,tlimit=0x1
    //wcn_chr_set_event,module=BT,event_id=xx,set=0

    if(skt_read_value <= 0) {
        SNOOPE("bt read from wcn_chr server failed");
        goto exit_p;
    } else {
        SNOOPD("chr_skt_fd = %d, skt_read_value = %d, buffer = %s",
                chr_skt_fd,skt_read_value,buffer);
    }

    //if skt_read_value >= 0  :read something from socket   ----enable or disable chr
    bytes = sizeof(buffer);
    read_msg = explode(',', buffer, &bytes);

    if (select_num_from_string(read_msg[3]) == 0) {
         SNOOPD("bt chr stop count");
         bluetooth_chr_clear_chr_ind();
	 goto exit_p;
    }

    if (select_num_from_string(read_msg[3]) == 1) {
         //in case buffer change to:
         //wcn_chr_disablent,module=BT,event_id=0x30002,set=1,maxcount=0x100,tlimit=0x1
         bluetooth_chr_clear_chr_ind();
         memset(buffer, 0 ,sizeof(buffer));
         SNOOPD("bt chr count");
	do {
            FD_SET(chr_skt_fd, &fds);
            int ret = select(chr_skt_fd + 1, &fds, NULL, NULL, &timeout);
            if(ret <= 0) {
                goto count_p;
            }

            skt_read_value = skt_read(chr_skt_fd, buffer, sizeof(buffer));
            SNOOPD("chr_skt_fd = %d, skt_read_value = %d, buffer = %s",
                chr_skt_fd,skt_read_value,buffer);
            if (0 == strcmp(buffer, WCN_CHR_SOCKET_CMD_DISABLE)) {
                bluetooth_release_skt(chr_skt_fd);
                msg.ref_count = 0;
                bt_chr_event_bitmap_30002 = 0;
                goto connect_p;
            }
count_p:
 	   //SNOOPD("still count");

            if (msg.ref_count < bt_chr_event_bitmap_30002) {
                SNOOPD("recv msg from firmware msg.ref_count: %d, bt_chr_event_bitmap_30002:%d, chr_ind_30002:%s",
                        msg.ref_count, bt_chr_event_bitmap_30002,chr_ind_30002);
                msg.ref_count = bt_chr_event_bitmap_30002;
                msg.event_id = 0x23002;
                msg.event_content = chr_ind_30002;
                msg.event_content_len = strlen(chr_ind_30002);
                sprintf(temp_ind_event,"wcn_chr_ind_event,module=BT,ref_count=%d,event_id=0x%x,version=%d,event_content_len=%d,char_info=%s",
                            msg.ref_count, msg.event_id, msg.version, msg.event_content_len, msg.event_content);
                SNOOPD("temp_ind_event: %s",temp_ind_event);
                skt_write_value = write(chr_skt_fd, temp_ind_event, strlen(temp_ind_event));

                if (skt_write_value < 0) {
                    SNOOPE("bt write to wcn_chr server failed");
                    bluetooth_chr_clear_chr_ind();
		    goto exit_p;
                }
            }
        //power save
        sleep(5);
        } while (thread_isrunning);
    }
    //release chr_skt_fd
    bluetooth_release_skt(chr_skt_fd);

exit_p:
    SNOOPD("exit");
    bluetooth_release_skt(chr_skt_fd);
}


void bluetooth_chr_init(void)
{
    int ret;
    //pthread_t thread_id;
    thread_isrunning = TRUE;

    ret = pthread_create(&thread_id, NULL,(void*)bluetooth_chr_thread, NULL);
    if (ret != 0) {    
    	SNOOPE("bluetooth_chr_init pthread_create failed");
        thread_isrunning = FALSE;
    }
}


void bluetooth_chr_cleanup(void)
{
    shutdown(chr_skt_fd, SHUT_RDWR);
    SNOOPD("wait bluetooth_chr_thread exit ");
    usleep(1*1000);
    thread_isrunning = FALSE;
    //if chr not ind disable, will not clear bt_chr_event_bitmap_30002
    //bt_chr_event_bitmap_30002 = 0;

    int ret = pthread_kill(thread_id, SIGUSR2);
    SNOOPD("pthread_kill %d", ret);

    if ( 0 == ret ) {
        SNOOPD("bluetooth_chr_thread exit complete ");
    }
}

