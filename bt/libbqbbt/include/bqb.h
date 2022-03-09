/**
 * Copyright (C) 2016 Spreadtrum Communications Inc.
  **/

#ifndef BT_LIBBQBBT_INCLUDE_BQB_H_
#define BT_LIBBQBBT_INCLUDE_BQB_H_

#include <stdint.h>
#include <semaphore.h>

#define BQB_CTRL_PATH "/data/misc/.bqb_ctrl"
#define ROUTE_BQB 0
#define ROUTE_AT 1
#define ENABLE_BQB_TEST "AT+SPBQBTEST=1"
#define DISABLE_BQB_TEST "AT+SPBQBTEST=0"
#define TRIGGER_BQB_TEST "AT+SPBQBTEST=?"
#define NOTIFY_BQB_ENABLE "\r\n+SPBQBTEST OK: ENABLED\r\n"
#define NOTIFY_BQB_DISABLE "\r\n+SPBQBTEST OK: DISABLE\r\n"
#define TRIGGER_BQB_ENABLE "\r\n+SPBQBTEST OK: BQB\r\n"
#define TRIGGER_BQB_DISABLE "\r\n+SPBQBTEST OK: AT\r\n"
#define UNKNOW_COMMAND "\r\n+SPBQBTEST ERROR: UNKNOW COMMAND\r\n"

#define ENABLE_WCNIT_TEST "AT+SPBQBTEST=2"
#define NOTIFY_WCNIT_ENABLE "\r\n+SPBQBTEST WCNIT OK: ENABLED\r\n"

#define SETDATA_PERF_TEST "AT+SPRDTESTDATA"
#define START_PERF_TEST "AT+SPRDTESTSTART"
#define STOP_PERF_TEST "AT+SPRDTESTSTOP"

#define NOTIFY_SETDATA_PERF "\r\n+SPRDTESTDATA OK\r\n"
#define NOTIFY_START_PERF "\r\n+SPRDTESTSTART OK\r\n"
#define NOTIFY_STOP_PERF "\r\n+SPRDTESTSTOP OK\r\n"
#define NOTIFY_SETDATA_ERROR "\r\n+SPRDTESTDATA ERROR\r\n"
#define NOTIFY_START_ERROR "\r\n+SPRDTESTSTART ERROR\r\n"
#define NOTIFY_STOP_ERROR "\r\n+SPRDTESTSTOP ERROR\r\n"


#define UNUSED_ATTR __attribute__((unused))
#define MAX_LINKS 7

typedef struct {
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
    uint8_t data[];
} BT_HDR;


typedef enum {
    BQB_NORMAL,
    BQB_NOPSKEY,
} bt_bqb_mode_t;

typedef enum {
    BQB_CLOSED,
    BQB_OPENED,
} bt_bqb_state_t;

typedef struct {
    uint16_t pkt_len;
    uint16_t handle;
    sem_t pkt_num;
    uint16_t pkt_cnt;
    uint8_t data[2048];
    timer_t timer_id;
    bool timer_created;
    bool in_use;
    pthread_t ntid;
    sem_t credit_sem;
} bt_perf_entity_t;

typedef struct {
    uint16_t acl_num;
    uint16_t sco_num;
    uint16_t link_num;
    bool is_buf_checked;
    bt_perf_entity_t link[MAX_LINKS];
} bt_perf_dev_t;

typedef enum {
    PERF_STOPPED,
    PERF_STARTED,
} bt_perf_state_t;

typedef void (*alarm_cb)(union sigval v);

/*
 * Bluetooth BQB Channel Control Interface
 */
typedef struct {
    /** Set to sizeof(bt_vndor_interface_t) */
    size_t          size;

    /*
     * Functions need to be implemented in Vendor libray (libbt-vendor.so).
     */

    /**
     * Caller will open the interface and pass in the callback routines
     * to the implemenation of this interface.
     */
    void (*init)(void);

    /**  Vendor specific operations */
    void (*exit)(void);
    void (*set_fd)(int fd);
    int (*get_bqb_state)(void);
    int (*check_received_str)(int fd, char* engbuf, int len);
    void (*eng_send_data)(char *engbuf, int len);
} bt_bqb_interface_t;

int current_bqb_state;
int current_perf_state;

int bt_on(bt_bqb_mode_t mode);
void bt_off(void);

typedef int (*controller2uplayer_t)(char * buf, unsigned int len);

void lmp_assert(void);
void lmp_deassert(void);

int get_bqb_state(void);
void set_bqb_state(int state);

#endif  // BT_LIBBQBBT_INCLUDE_BQB_H_
