#ifndef LOG_TAG
#define LOG_TAG "bt_sprd_snoop_tag"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include <pthread.h>
#include <utils/Log.h>
#include <sys/eventfd.h>
#include <cutils/sockets.h>
#include <inttypes.h>

#define HCI_EDR3_DH5_PACKET_SIZE    1021
#define BTSNOOP_HEAD_SIZE  25
#define HCI_PIIL_SVAE_BUF_SIZE 5
#define HCI_POLL_SIZE (HCI_EDR3_DH5_PACKET_SIZE + BTSNOOP_HEAD_SIZE + HCI_PIIL_SVAE_BUF_SIZE)

#define UNUSED(x) (void)(x)

#define INVALID_FD (-1)
#define EFD_SEMAPHORE (1 << 0)

//static int file_fd = -1;
static pthread_t pop_thread;

pthread_mutex_t lock;
pthread_mutex_t cap_lock;

typedef struct {
    uint8_t buf[HCI_POLL_SIZE];
    uint8_t data[HCI_POLL_SIZE * 2];
    uint32_t len;
    uint8_t completion;
} HCI_POOL_T;

typedef struct list_node_t {
    struct list_node_t *next;
    void *data;
    uint32_t len;
    uint64_t timestamp;
} list_node_t;

typedef struct {
    list_node_t *head;
    list_node_t *tail;
    uint32_t count;
    uint64_t size;
} list_t;

static list_t hcidata_queue = {
    .head = NULL,
    .tail = NULL,
    .count = 0,
    .size = 0
};

static HCI_POOL_T hci_pool;

static int semaphore_fd = INVALID_FD;

static const uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;

uint8_t list_append_node(list_t *list, list_node_t *node);
static list_node_t *list_pop_node(list_t *list);
static inline void semaphore_new(void);
//static inline void semaphore_free(void);
static inline void semaphore_wait(void);
static inline void semaphore_post(void);

static uint64_t btsnoop_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Timestamp is in microseconds.
    uint64_t timestamp = tv.tv_sec * 1000 * 1000LL;
    timestamp += tv.tv_usec;
    timestamp += BTSNOOP_EPOCH_DELTA;
    return timestamp;
}

/*static int create_log_file(void) {
    int fd;
    int ret = 0;
    char file_name[256] = {0};
    struct tm timenow, *ptm;
    struct timeval tv;
    static int fileHeaderWrited=0;

    gettimeofday(&tv, NULL);
    ptm = localtime_r(&tv.tv_sec, &timenow);
    strftime(file_name, sizeof(file_name), "/data/misc/bluetooth/btsnoop.%Y-%m-%d_%H.%M.%S.log", ptm);

    fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, S_IRUSR|S_IWUSR);
    if(fd < 0) {
       ALOGD("open file fail");
       return -1;
    }
    if(0 == fileHeaderWrited){
        fileHeaderWrited = 1;
        if (write(fd, "btsnoop\0\0\0\0\1\0\0\x3\xea", 16) < 0) {
            ALOGD("write btsnoop header failed: %d", ret);
        }
     }

    return fd;
}*/

static void dump_hex(uint8_t *src, int len) {
#define DUMP_LINE_MAX 2048
    int i, j, buf_size;
    char *dump_buf = NULL;
    buf_size = DUMP_LINE_MAX * strlen("FF ");
    dump_buf = (char *)malloc(buf_size);
    if (len < DUMP_LINE_MAX) {
        for (i = 0; i < len; i++) {
            snprintf(dump_buf + 3 * i, buf_size, "%02X ", src[i]);
        }
        dump_buf[len * 3 - 1] = '\0';
        ALOGD("  %s", dump_buf);
    } else {
        for (i = 0; i < len / DUMP_LINE_MAX; i++) {
            for (j = 0; j < DUMP_LINE_MAX; j++) {
                snprintf(dump_buf + 3 * j, buf_size, "%02X ", src[i * DUMP_LINE_MAX + j]);
            }
            dump_buf[DUMP_LINE_MAX * 3 - 1] = '\0';
            ALOGD("  %s", dump_buf);
        }
        if (len % DUMP_LINE_MAX) {
            for (j = 0; j < len % DUMP_LINE_MAX; j++) {
                snprintf(dump_buf + 3 * j, buf_size, "%02X ", src[(i - 1) * DUMP_LINE_MAX + j]);
            }
            if (len % DUMP_LINE_MAX >= 1) {
                dump_buf[len % DUMP_LINE_MAX * 3 - 1] = '\0';
            }
            ALOGD("  %s", dump_buf);
        }
    }

    free(dump_buf);
    dump_buf = NULL;
}

static inline void semaphore_new(void) {
    if (semaphore_fd != INVALID_FD) {
        close(semaphore_fd);
    }
    semaphore_fd = eventfd(0, EFD_SEMAPHORE);
    if (semaphore_fd == INVALID_FD) {
        ALOGD("error");
    }
}

static inline void semaphore_free(void) {
    if (semaphore_fd != INVALID_FD)
        close(semaphore_fd);
}

static inline void semaphore_wait(void) {
    uint64_t value;
    assert(semaphore_fd != INVALID_FD);
    if (eventfd_read(semaphore_fd, &value) == -1) {
        ALOGD("%s unable to wait on semaphore: %s", __func__, strerror(errno));
    }
}

static inline void semaphore_post(void) {
    assert(semaphore_fd != INVALID_FD);
    if (eventfd_write(semaphore_fd, 1ULL) == -1) {
        ALOGD("%s unable to post to semaphore: %s", __func__, strerror(errno));
    }
}

uint8_t list_append_node(list_t *list, list_node_t *node) {
    assert(list != NULL);
    assert(data != NULL);

    if (!node) {
        ALOGD("malloc failed");
        return false;
    }

    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size += node->len;
    ++list->count;
    return true;
}

static list_node_t *list_pop_node(list_t *list) {
    assert(list != NULL);

    list_node_t *node = NULL;

    if (list->head != NULL) {
        node = list->head;
        if (list->head == list->tail) {
            list->tail = list->head->next;
        }
        list->head = list->head->next;
        --list->count;
        list->size -= node->len;
    } else {
        ALOGD("not found list head");
    }
    return node;
}

static void *btsnoop_pop_thread(void* arg) {
    //int ret = -1;
    uint64_t write_time, done_time;
    UNUSED(arg);
    /*if (file_fd != -1) {
        close(file_fd);
    }
    file_fd = create_log_file();
    if(file_fd < 0) {
        ALOGD("create log file failed");
        return NULL;
    }*/
    uint64_t file_size = 0;
    do {
        semaphore_wait();
        if (!hcidata_queue.count) {
            ALOGD("hcidata_queue.count: %d", (int)hcidata_queue.count);
        }
        pthread_mutex_lock(&lock);
        list_node_t *node = list_pop_node(&hcidata_queue);
        pthread_mutex_unlock(&lock);
        if (node == NULL) {
            ALOGD("pop failed");
            return NULL;
        }

        write_time = btsnoop_timestamp();
        //ret = write(file_fd, node->data, node->len);
        dump_hex((uint8_t*)(node->data), node->len);
        done_time = btsnoop_timestamp();

        if ((done_time - write_time) > (100 * 1000LL)) {
            uint64_t before = done_time - write_time;
            uint64_t after = done_time - (node->timestamp - BTSNOOP_EPOCH_DELTA);
            ALOGD("write block: %" PRIu64 ": %" PRIu64 " us", before, after);
        }

        /*if (ret <= 0) {
            ALOGD("write failed: %d", ret);
            free(node);
            node = NULL;
            continue;
        }*/

        file_size += node->len;

        free(node);
        node = NULL;
    } while (1);
    return NULL;
}

static int parse_and_save(unsigned char* data_ptr, int data_length) {
    int type;
    uint32_t length_a, length_b;
    hci_pool.len = data_length;
    memcpy(hci_pool.buf, data_ptr, data_length);
    memcpy(hci_pool.data, hci_pool.buf, data_length);
    //ALOGD("hci_pool.len: %d", hci_pool.len);
    do {
        if (hci_pool.len < BTSNOOP_HEAD_SIZE) {
            //ALOGD("not reach head size");
            break;
        }
        length_a = ((uint32_t)(hci_pool.data[0] & 0xFF)) << 24
                 | ((uint32_t)(hci_pool.data[1] & 0xFF)) << 16
                 | ((uint32_t)(hci_pool.data[2] & 0xFF)) << 8
                 | ((uint32_t)(hci_pool.data[3] & 0xFF)) << 0;
        length_b = ((uint32_t)(hci_pool.data[4] & 0xFF)) << 24
                 | ((uint32_t)(hci_pool.data[5] & 0xFF)) << 16
                 | ((uint32_t)(hci_pool.data[6] & 0xFF)) << 8
                 | ((uint32_t)(hci_pool.data[7] & 0xFF)) << 0;
        type = hci_pool.data[24];
        if (length_a == length_b) {
            //ALOGD("length: %d", length_a);
            if (length_a > ((uint32_t)sizeof(hci_pool.data) - BTSNOOP_HEAD_SIZE)) {
                //ALOGD("unknow head");
                return -1;
            }
            if (hci_pool.len < length_a + BTSNOOP_HEAD_SIZE - 1) {
                //ALOGD("incomplete");
                break;
            }
            switch (type) {
                case 1:
                    if (length_a == (uint32_t)hci_pool.data[27] + 4) {
                        hci_pool.completion = 1;
                        //ALOGD("COMMAND");
                    }
                    break;
                case 2:
                    if (length_a == (uint32_t)hci_pool.data[27] + 5 + (((uint32_t)hci_pool.data[28]) << 8)) {
                        hci_pool.completion = 1;
                        //ALOGD("ACL");
                    }
                    break;
                case 3:
                    if (length_a == (uint32_t)hci_pool.data[27] + 4) {
                        hci_pool.completion = 1;
                        //ALOGD("SCO");
                    }
                    break;
                case 4:
                    if (length_a == (uint32_t)hci_pool.data[26] + 3) {
                        hci_pool.completion = 1;
                        //ALOGD("EVENT");
                    }
                    break;
                default:
                    //ALOGD("unknow type: %d", type);
                break;
            }
        } else {
            //ALOGD("length_a=%08x, length_b=%08x", length_a, length_b);
        }
        if (hci_pool.completion) {
            list_node_t *node = (list_node_t *)malloc(sizeof(list_node_t) + (length_a + BTSNOOP_HEAD_SIZE -1));
            node->data = (uint8_t*)(node +1);
            memcpy(node->data, hci_pool.data, (length_a + BTSNOOP_HEAD_SIZE -1));
            node->next = NULL;
            node->len = (length_a + BTSNOOP_HEAD_SIZE -1);
            node->timestamp = ((uint64_t)(hci_pool.data[16] & 0xFF)) << 56
                            | ((uint64_t)(hci_pool.data[17] & 0xFF)) << 48
                            | ((uint64_t)(hci_pool.data[18] & 0xFF)) << 40
                            | ((uint64_t)(hci_pool.data[19] & 0xFF)) << 32
                            | ((uint64_t)(hci_pool.data[20] & 0xFF)) << 24
                            | ((uint64_t)(hci_pool.data[21] & 0xFF)) << 16
                            | ((uint64_t)(hci_pool.data[22] & 0xFF)) << 8
                            | ((uint64_t)(hci_pool.data[23] & 0xFF)) << 0;
            pthread_mutex_lock(&lock);
            list_append_node(&hcidata_queue, node);
            pthread_mutex_unlock(&lock);
            semaphore_post();
            hci_pool.len -= (length_a + BTSNOOP_HEAD_SIZE -1);
            //ALOGD("len: %d, save: %d", hci_pool.len, (length_a + BTSNOOP_HEAD_SIZE -1));
            if (hci_pool.len) {
                memmove(hci_pool.data, &hci_pool.data[length_a + BTSNOOP_HEAD_SIZE -1], hci_pool.len);
            }
            //ALOGD("copy complete");
            hci_pool.completion = 0;
            //continue;
        } else {
            break;
        }
    } while (1);
    return 0;
}

void bt_snoop_packet_cap(int type, const uint8_t *packet, bool is_received)
{
    int length_he = 0;
    int length;
    int flags;
    int drops = 0;
    unsigned char hci_pool[HCI_POLL_SIZE] = {0};
    uint64_t snoop_timestamp = 0;

    switch (type)
    {
        case 1:
            length_he = packet[2] + 4;
            flags = 2;
            break;

        case 2:
            length_he = (packet[3] << 8) + packet[2] + 5;
            flags = is_received;
            break;

        case 3:
            length_he = packet[2] + 4;
            flags = is_received;
            break;

        case 4:
            length_he = packet[1] + 3;
            flags = 3;
            break;
    }

    snoop_timestamp = btsnoop_timestamp();
    uint32_t time_hi = snoop_timestamp >> 32;
    uint32_t time_lo = snoop_timestamp & 0xFFFFFFFF;

    length = htonl(length_he);
    flags = htonl(flags);
    drops = htonl(drops);
    time_hi = htonl(time_hi);
    time_lo = htonl(time_lo);

    memcpy(hci_pool, &length, 4);
    memcpy(hci_pool + 4, &length, 4);
    memcpy(hci_pool + 8, &flags, 4);
    memcpy(hci_pool + 12, &drops, 4);
    memcpy(hci_pool + 16, &time_hi, 4);
    memcpy(hci_pool + 20, &time_lo, 4);
    memcpy(hci_pool + 24, &type, 1);
    memcpy(hci_pool + 25, packet, length_he - 1);
    //ALOGD("bt_snoop_packet_cap lock");
    pthread_mutex_lock(&cap_lock);
    parse_and_save(hci_pool, BTSNOOP_HEAD_SIZE + length_he - 1);
    pthread_mutex_unlock(&cap_lock);
    //ALOGD("bt_snoop_packet_cap unlock");
}

void bt_snoop_thread_start(void) {
    //ALOGD("start");
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&cap_lock, NULL);
    semaphore_new();
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&pop_thread, &attr,
                       btsnoop_pop_thread, NULL) != 0) {
        ALOGE("create error");
    }
    //ALOGD("out");
}
void bt_snoop_thread_stop(void) {
    //ALOGD("stop");
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&cap_lock);
    semaphore_free();
    pthread_join(pop_thread, NULL);
    //ALOGD("out");
}
