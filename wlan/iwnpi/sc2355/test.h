#ifndef __WLNPI_TEST__
#define __WLNPI_TEST__

#include <stdio.h>
#include <android/log.h>
#include <utils/Log.h>

typedef struct android_wifi_priv_cmd {
	char *buf;
	int  used_len;
	int  total_len;
} android_wifi_priv_cmd;

int sprdwl_handle_test(int argc, char **argv);
int sprdwl_handle_set_sar(int argc, char **argv);

#endif
