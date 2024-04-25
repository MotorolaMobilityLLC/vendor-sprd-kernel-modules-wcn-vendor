/******************************************************************************
 *
 * This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.
 *
 *  Copyright 1999-2012 Broadcom Corporation
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
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>

#include <getopt.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <cutils/properties.h>
#define BUILD_TYPE_PROP_KEY "ro.build.type"
#define USER_DEBUG_VERSION_STR "userdebug"



// ********** ***********FM IOCTL define start *******************************
#define FM_IOC_MAGIC        0xf5

#define FM_IOCTL_POWERUP       _IOWR(FM_IOC_MAGIC, 0, struct fm_tune_parm*)
#define FM_IOCTL_POWERDOWN     _IOWR(FM_IOC_MAGIC, 1, int32_t*)
#define FM_IOCTL_TUNE          _IOWR(FM_IOC_MAGIC, 2, struct fm_tune_parm*)
#define FM_IOCTL_SEEK          _IOWR(FM_IOC_MAGIC, 3, struct fm_seek_parm*)
#define FM_IOCTL_SETVOL        _IOWR(FM_IOC_MAGIC, 4, uint32_t*)
#define FM_IOCTL_GETVOL        _IOWR(FM_IOC_MAGIC, 5, uint32_t*)
#define FM_IOCTL_MUTE          _IOWR(FM_IOC_MAGIC, 6, uint32_t*)
#define FM_IOCTL_GETRSSI       _IOWR(FM_IOC_MAGIC, 7, int32_t*)
#define FM_IOCTL_SCAN          _IOWR(FM_IOC_MAGIC, 8, struct fm_scan_parm*)
#define FM_IOCTL_STOP_SCAN     _IO(FM_IOC_MAGIC,   9)

#define FM_IOCTL_RW_REG        _IOWR(FM_IOC_MAGIC, 0xC, struct fm_reg_ctl_parm*)
#define FM_IOCTL_GET_SNR       _IOWR(FM_IOC_MAGIC, 0x4D, int32_t*)


#define FM_DEV_NAME "/dev/fm"
#define FM_SEEK_SPACE 1
// auto HiLo
#define FM_AUTO_HILO_OFF    0
#define FM_AUTO_HILO_ON     1

// seek direction
#define FM_SEEK_UP          0
#define FM_SEEK_DOWN        1

struct fm_tune_parm {
    uint8_t err;
    uint8_t band;
    uint8_t space;
    uint8_t hilo;
    uint16_t freq;
};

struct fm_seek_parm {
    uint8_t err;
    uint8_t band;
    uint8_t space;
    uint8_t hilo;
    uint8_t seekdir;
    uint8_t seekth;
    uint16_t freq;
};

struct fm_reg_ctl_parm {
    unsigned char reg;
    unsigned int addr;
    unsigned int val;
    /*0:write, 1:read*/
    unsigned char rw_flag;
} __packed;

typedef enum {
    ACTION_NONE,
    ACTION_READ,
    ACTION_WRITE,
} fmtools_action_t;

static char *action_string[] = {"UNKNOWN", "Read", "Write"};

typedef enum {
    TARGET_NONE,
    TARGET_BB,
    TARGET_RF,
} fmtools_target_t;

static char *target_string[] = {"NONE", "FM BB register", "FM RF register"};

static fmtools_action_t fm_action = ACTION_NONE;
static fmtools_target_t fm_target = TARGET_NONE;

static void usage(void){
    printf(
    "Usage: fm_tools [OPTION...] [filter]\n"
    "  -R, --Read\n"
    "  -w, --Write\n"
    "  -o, --open\n"
    "  -p, --powerup\n"
    "  -d, --powerdown\n"
    "  -s, --seek\n"
    "  -t, --tune\n"
    "  -m, --mute\n"
    "  -u, --unmute\n"
    "  -g, --get rssi\n"
    "      --usage                Give a short usage message\n"
    );
}


static void hrw_usage(void){
    printf(
    "Usage: fm_tools [OPTION...] [filter]\n"
    "fm_tools -r rf  0x01b9\n"
    "fm_tools -w rf  0x01b9 0x01\n"
    "fm_tools -r bb  0x50040600\n"
    "fm_tools -w bb  0x50040600 0x01 \n"
    );
}

static struct option main_options[] = {
    { "Read",         required_argument, 0, 'r' },
    { "Write",        required_argument, 0, 'w' },
    { "target",            1, 0, 't' },
    { "help",            0, 0, 'h' },
    { 0 , 0, 0, 0}
};

static void get_target(char *target_t)
{
    if (!strcmp(target_t, "bb")) {
        fm_target = TARGET_BB;
    } else if (!strcmp(target_t, "rf")) {
        fm_target = TARGET_RF;
    }
}



int main(int argc, char* argv[])
{
    int fd = -1, ret = 0;
    int power_up = 0;
    int retpu =0;

    int opt;
    char *fm_optarg;
    struct fm_reg_ctl_parm  parm;
	struct fm_seek_parm rx_parm;
	rx_parm.freq = 9140;
    char value[92] = {'\0'};
    property_get(BUILD_TYPE_PROP_KEY, value, "");

	if (argc == 1)
		usage();
	while (((opt=getopt_long(argc, argv, "r:w:ocpdstmug", main_options, NULL)) != -1) && strstr(value,USER_DEBUG_VERSION_STR)) {//nargc, nargv, options, long_options, index
		switch(opt) {
			case 'r':
				fm_action = ACTION_READ;
				get_target(optarg);
				break;

			case 'w':
				fm_action = ACTION_WRITE;
				get_target(optarg);
				break;

			case 'o' : {
				printf("------------------- Test Open Fm Device ---------------------\n");
				fd = open(FM_DEV_NAME, O_RDWR);
				if (fd < 0) {
					printf("Open %s failed, error: %s\n", FM_DEV_NAME, strerror(errno));
				} else {
					printf("Open %s successful, fd : %d\n", FM_DEV_NAME, fd);
				}
				printf("------------------- Test Open Fm Device End -----------------\n\n");
				break;
			}

			case 'p' : {
				printf("------------------- Test Fm Power On ---------------------\n");
				struct fm_tune_parm parm_powerup;

				parm_powerup.band = 1;    /* 1 for default bandrand US  87.5 - 108*/

				parm_powerup.freq = 9750;
				parm_powerup.hilo = FM_AUTO_HILO_OFF;
				parm_powerup.space = FM_SEEK_SPACE;

				if (fd >= 0) {
					ret = ioctl(fd, FM_IOCTL_POWERUP, &parm_powerup);
					if (ret == 0) {
						power_up = 1;
						printf("Power Up successful, ret: %d\n", ret);
					} else {
						printf("Power Up failed, ret: %d\n", ret);
					}
				} else {
					printf("Fm Device havn't opnned.\n");
				}
				printf("------------------- Test Fm Power On End -----------------\n\n");
				break;
			}

			case 'd' : {
				printf("------------------- Test Fm Power Down ---------------------\n");
				int type = 0;
				if (fd >= 0) {
					ret = ioctl(fd, FM_IOCTL_POWERDOWN, &type);
					if (ret == 0) {
						power_up = 0;
						printf("Power Down successful, ret: %d\n", ret);
					} else {
						printf("Power Down failed, ret: %d\n", ret);
					}
				} else {
					printf("Fm Device havn't opnned.\n");
				}
				printf("------------------- Test Fm Power Down End -----------------\n\n");
				break;
			}

			case 's' : {
				printf("------------------- Test Fm Seek ---------------------\n");
				struct fm_seek_parm parm_seek;

				parm_seek.band = 1;
				parm_seek.freq = 1000;
				parm_seek.hilo = FM_AUTO_HILO_OFF;
				parm_seek.space = FM_SEEK_SPACE;
				parm_seek.seekdir = FM_SEEK_UP;
				parm_seek.seekth = 0;

				if (fd >= 0 && power_up) {
					ret = ioctl(fd, FM_IOCTL_SEEK, &parm_seek);
					if (ret == 0) {
						rx_parm.freq = parm_seek.freq;
						printf("Seek successful, ret: %d, seek station:%d \n", ret,rx_parm.freq);
					} else {
						printf("Seek failed, ret: %d\n", ret);
					}
				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm Seek End -----------------\n\n");
				break;
			}

			case 't' : {
				printf("------------------- Test Fm tune ---------------------\n");
				struct fm_tune_parm parm_tune;

				parm_tune.band = 1;    /* 1 for default bandrand US  87.5 - 108*/
				parm_tune.freq = rx_parm.freq;//add juge tune pram
				parm_tune.freq = strtoul(argv[optind], 0, 0);
				parm_tune.hilo = FM_AUTO_HILO_OFF;

				if (fd >= 0 && power_up) {
					ret = ioctl(fd, FM_IOCTL_TUNE, &parm_tune);
					if (ret == 0) {
						printf("tune successful, ret: %d,freq is %d\n", ret,parm_tune.freq);
					} else {
						printf("tune failed, ret: %d\n", ret);
					}
				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm tune End -----------------\n\n");
				break;
			}

			case 'm' : {
				printf("------------------- Test Fm mute ---------------------\n");
				int temp_mute = 1;

				if (fd >= 0 && power_up) {
					ret = ioctl(fd, FM_IOCTL_MUTE, &temp_mute);
					if (ret == 0) {
						printf("mute successful, ret: %d\n", ret);
					} else {
						printf("mute failed, ret: %d\n", ret);
					}
				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm mute End -----------------\n\n");
				break;
			}

			case 'u' : {
				printf("------------------- Test Fm unmute ---------------------\n");
				int temp_mute = 0;

				if (fd >= 0 && power_up) {
					ret = ioctl(fd, FM_IOCTL_MUTE, &temp_mute);
					if (ret == 0) {
						printf("unmute successful, ret: %d\n", ret);
					} else {
						printf("unmute failed, ret: %d\n", ret);
					}
				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm unmute End -----------------\n\n");
				break;
			}

			case 'g' :  {
				printf("------------------- Test Fm get rssi ---------------------\n");
				int temp = 0;
				int soft_rssi[10] = {0};
				int soft_rssi_num = 10;
				int rssi = 0;
				int flag = 1;

				if (fd >= 0 && power_up) {


					for(int i = 0; i < soft_rssi_num; i++){
					ret = ioctl(fd, FM_IOCTL_GETRSSI, &temp);
					if (ret == 0){
						soft_rssi[i] = temp;

					} else {
						printf("get rssi failed, ret: %d\n", ret);
						flag = 0;
						break;
					}
					rssi = soft_rssi[i] + rssi ;

					}
					if(flag != 0){
					printf("get rssi successful!,rssi: -%d\n", rssi/soft_rssi_num);
					}


				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm get rssi End -----------------\n\n");
				break;
			}
			case 'n' : {
				printf("------------------- Test Fm get snr ---------------------\n");


				int snr = 0;
				if (fd >= 0 && power_up) {



				    ret = ioctl(fd, FM_IOCTL_GET_SNR, &snr);
					if (ret == 0) {
						printf("get snr successful!,snr: -%d\n", snr);
					} else {
						printf("get snr failed, ret: %d\n", ret);
					}
				} else if (fd < 0) {
					printf("Fm Device havn't opnned.\n");
				} else {
					printf("Fm Device is power down.\n");
				}
				printf("------------------- Test Fm get snr End -----------------\n\n");
				break;
			}

			default:
			usage();
			printf("\n#############\n");
			hrw_usage();
		}
	}
	switch (fm_action) {
		case ACTION_READ:
			fd = open(FM_DEV_NAME, O_RDWR);
			parm.reg = 0;
			parm.addr = strtoul(argv[optind], 0, 0);
			parm.val = 0;

			switch (fm_target) {
				case TARGET_BB:
					parm.rw_flag = 1;
					break;

				case TARGET_RF:
					parm.rw_flag = 3;
					break;

				default:
					parm.rw_flag = 5;
					break;
			}

			printf("FM reg=%d, addr=0x%x, value =0x%x, rw_flag=%d\n",\
					parm.reg, parm.addr, parm.val, parm.rw_flag);

			if (fd >= 0) {
				ret = ioctl(fd, FM_IOCTL_RW_REG, &parm);
				if (ret == 0) {
					printf("read register successful:addr=0x%x,value=0x%x\n", parm.addr, parm.val);
				} else {
					printf("read register failed, ret: %d\n", ret);
				}
			} else {
				printf("Fm Device havn't opnned.\n");
			}
			break;

		case ACTION_WRITE:
			fd = open(FM_DEV_NAME, O_RDWR);
			parm.reg = 0;
			parm.addr = strtoul(argv[optind], 0, 0);
			parm.val=strtoul(argv[optind+1], 0, 0);

			switch (fm_target) {
				case TARGET_BB:
					parm.rw_flag = 0;
					break;

				case TARGET_RF:
					parm.rw_flag = 2;
					break;

				default:
					parm.rw_flag = 4;
					break;
			}

			printf("FM reg=%d, addr=0x%x, value =0x%x, rw_flag=%d\n",\
					parm.reg, parm.addr, parm.val, parm.rw_flag);

			if (fd >= 0) {
				ret = ioctl(fd, FM_IOCTL_RW_REG, &parm);
				if (ret == 0) {
					printf("write register successful:addr=0x%x,value=0x%x\n", parm.addr, parm.val);
				} else {
					printf("write register failed, ret: %d\n", ret);
				}
			} else {
				printf("Fm Device havn't opnned.\n");
			}
			break;

		default:
			break;
	}
	if (fd >= 0) {
		close(fd);
		printf("free fd\n");
	}
	return 1;
}
