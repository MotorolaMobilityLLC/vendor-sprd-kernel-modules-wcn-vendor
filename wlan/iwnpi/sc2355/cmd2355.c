/*
 * Authors:<jay.yang@spreadtrum.com>
 * Owner:
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#include "wlnpi.h"

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>
#include <endian.h>
#include <linux/types.h>

#include <android/log.h>
#include <utils/Log.h>


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG     "WLNPI"

#define ENG_LOG  ALOGD

#define WLNPI_RESULT_BUF_LEN        (256)

#define IWNPI_EXEC_TMP_FILE           ("/mnt/vendor/iwnpi_exec_data.log")
#define IWNPI_EXEC_BEAMF_STATUS_FILE  ("/mnt/vendor/iwnpi_exec_beamf_status_data.log")
#define IWNPI_EXEC_RXSTBC_STATUS_FILE ("/mnt/vendor/iwnpi_exec_rxstbc_status_data.log")

#define WLAN_EID_SUPP_RATES         (1)
#define WLAN_EID_HT_CAP             (45)
#define WLAN_EID_VENDOR_SPECIFIC    (221)
#define WPS_IE_VENDOR_TYPE          (0x0050f204)

#define IWNPI_CONN_MODE_MANAGED     ("Managed")
#define IWNPI_CONN_MODE_OTHER       ("Other Mode")
#define IWNPI_DEFAULT_CHANSPEC      ("2.4G")
#define IWNPI_DEFAULT_BANDWIDTH     ("20MHz")

char iwnpi_ret_buf[WLNPI_RESULT_BUF_LEN] = {0x00};

static int wlnpi_ap_info_print_capa(const char *data);
static int wlnpi_ap_info_print_support_rate(const char *data);
static char *wlnpi_get_rate_by_phy(int phy);
static char *wlnpi_bss_get_ie(const char *bss, char ieid);
static char *iwnpi_bss_get_vendor_ie(const char *bss, int vendor_type);
static int wlnpi_ap_info_print_ht_capa(const char *data);
static int wlnpi_ap_info_print_ht_mcs(const char *data);
static int wlnpi_ap_info_print_wps(const char *data);
//static int iwnpi_hex_dump(unsigned char *name, unsigned short nLen, unsigned char *pData, unsigned short len);

struct chan_t g_chan_list[] ={
	/*2.4G--20M*/
	{0,1,1},{1,2,2},{2,3,3},{3,4,4},{4,5,5},
	{5,6,6},{6,7,7},{7,8,8},{8,9,9},{9,10,10},
	{10,11,11},{11,12,12},{12,13,13},{13,14,14},
	/*2.4G---40M*/
	{14,1,3},{15,2,4},{16,3,5},{17,4,6},{18,5,7},
	{19,6,8},{20,7,9},{21,8,10},{22,9,11},{23,5,3},
	{24,6,4},{25,7,5},{26,8,6},{27,9,7},{28,10,8},
	{29,11,9},{30,12,10},{31,13,11},
	/*5G---20M*/
	{32,36,36},{33,40,40},{34,44,44},{35,48,48},{36,52,52},
	{37,56,56},{38,60,60},{39,64,64},{40,100,100},{41,104,104},
	{42,108,108},{43,112,112},{44,116,116},{45,120,120},{46,124,124},
	{47,128,128},{48,132,132},{49,136,136},{50,140,140},{51,144,144},
	{52,149,149},{53,153,153},{54,157,157},{55,161,161},{56,165,165},
	/*5G---40M*/
	{57,36,38},{58,40,38},{59,44,46},{60,48,46},{61,52,54},
	{62,56,54},{63,60,62},{64,64,62},{65,100,102},{66,104,102},
	{67,108,110},{68,112,110},{69,116,118},{70,120,118},{71,124,126},
	{72,128,126},{73,132,134},{74,136,134},{75,140,142},{76,144,142},
	{77,149,151},{78,153,151},{79,157,159},{80,161,159},
	/*5G---80M*/
	{81,36,42},{82,40,42},{83,44,42},{84,48,42},{85,52,58},
	{86,56,58},{87,60,58},{88,64,58},{89,100,106},{90,104,106},
	{91,108,106},{92,112,106},{93,116,122},{94,120,122},{95,124,122},
	{96,128,122},{97,132,138},{98,136,138},{99,140,138},{100,144,138},
	{101,149,155},{102,153,155},{103,157,155},{104,161,155},
};


/********************************************************************
*   name   iwnpi_get_be16
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static short iwnpi_get_be16(const char *a)
{
    return (a[0] << 8) | a[1];
}

/********************************************************************
*   name   iwnpi_get_le16
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static short iwnpi_get_le16(const char *a)
{
    return (a[1] << 8) | a[0];
}

/********************************************************************
*   name   iwnpi_get_be32
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int iwnpi_get_be32(const char *a)
{
    return (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

bool is_digital(const char *str)
{
    char first = *str;
    const char *tmp;

    if (first != '-' && first != '+' && (first < '0' || first > '9')) {
	return false;
    }

    tmp = str + 1;
    while(*tmp) {
	if (*tmp < '0' || *tmp > '9')
	    return false;
	tmp++;
    }
    return true;
}

int get_digit_base(char *val)
{
	int base = 10;

	if (val == NULL)
		return -1;

	if (val[0] == '0' && toupper(val[1]) == 'X')
		return 16;

	while (*val)
	{
		if (isxdigit(*val))
		{
			if(!isdigit(*val))
			{
				base = 16;
				break;
			}
		}
		else
		{
			base = -1;
			break;
		}

		val++;
	}

	return base;
}

int str2digit(char *str, unsigned long int *data)
{
	char *endptr = NULL;
	int base;
	unsigned long int val;

	base = get_digit_base(str);
	if (base < 0)
	{
		ENG_LOG("ADL leaving %s(), str = %s, get base error\n", __func__, str);
		return -1;
	}

	val = strtoul(str, &endptr, base);
	if (*endptr != '\0')
	{
		ENG_LOG("ADL leaving %s(), can not convert str(%s) to digital.", __func__, str);
		return -1;
	}

	*data = val;

	return 0;
}

static iwnpi_rate_table g_rate_table[] =
{
    {0x82, "1[b]"},
    {0x84, "2[b]"},
    {0x8B, "5.5[b]"},
    {0x0C, "6"},
    {0x12, "9"},
    {0x96, "11[b]"},
    {0x18, "12"},
    {0x24, "18"},
    {0x30, "24"},
    {0x48, "36"},
    {0x60, "48"},
    {0x6C, "54"},

    /* 11g */
    {0x02, "1[b]"},
    {0x04, "2[b]"},
    {0x0B, "5.5[b]"},
    {0x16, "11[b]"},
};

int mac_addr_a2n(unsigned char *mac_addr, char *arg)
{
    int i;

    for (i = 0; i < ETH_ALEN; i++)
    {
        int temp;
        char *cp = strchr(arg, ':');
        if (cp) {
            *cp = 0;
            cp++;
        }
        if (sscanf(arg, "%x", &temp) != 1)
            return -1;
        if (temp < 0 || temp > 255)
            return -1;

        mac_addr[i] = temp;
        if (!cp)
            break;
        arg = cp;
    }
    if (i < ETH_ALEN - 1)
        return -1;

    return 0;
}

int wlnpi_cmd_no_argv(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    if(argc > 0)
        return -1;
    *s_len = 0;
    return 0;
}

int wlnpi_show_only_status(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    return 0;
}

int wlnpi_show_only_int_ret(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(4 != r_len)
    {
        printf("%s() err\n", __func__);
        return -1;
    }
    printf("ret: %d :end\n",  *( (unsigned int *)(r_buf+0) )  );

    ENG_LOG("ADL leaving %s(), return 0", __func__);

    return 0;
}

/*-----CMD ID:0-----------*/
int wlnpi_cmd_start(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
	if(1== argc)
		mac_addr_a2n(s_buf, argv[0]);
	else
		memcpy(s_buf,  &(g_wlnpi.mac[0]),  6 );

	*s_len = 6;
    return 0;
}

/*-----CMD ID:2-----------*/
int wlnpi_cmd_set_mac(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    if(1 != argc)
        return -1;
    mac_addr_a2n(s_buf, argv[0]);
    *s_len = 6;
    return 0;
}

/*-----CMD ID:3-----------*/
int wlnpi_show_get_mac(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    int i, ret, p;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__,r_len);

    printf("ret: mac: %02x:%02x:%02x:%02x:%02x:%02x :end\n", r_buf[0], r_buf[1], r_buf[2],r_buf[3],r_buf[4], r_buf[5]);

    ENG_LOG("ADL leaving %s(), mac = %02x:%02x:%02x:%02x:%02x:%02x, return 0", __func__, r_buf[0], r_buf[1], r_buf[2],r_buf[3],r_buf[4], r_buf[5]);

    return 0;
}

/*-----CMD ID:4-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_macfilter
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_macfilter(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        ENG_LOG("ADL %s(), argc is ERROR, return -1", __func__);
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        ENG_LOG("ADL %s(), strtol is ERROR, return -1", __func__);
        return -1;
    }

    *s_len = 1;

    ENG_LOG("ADL leaving %s(), s_buf[0] = %d, s_len = %d", __func__, s_buf[0], *s_len);
    return 0;
}

/*-----CMD ID:5-----------*/
/********************************************************************
*   name   wlnpi_show_get_macfilter
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_macfilter(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char macfilter = 0;
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_macfilter err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    macfilter = *( (unsigned char *)(r_buf+0) ) ;
    printf("ret: %d :end\n", macfilter);

    ENG_LOG("ADL leaving %s(), macfilter = %d, return 0", __func__, macfilter);

    return 0;
}

/*-----CMD ID:6-----------*/
int wlnpi_cmd_set_channel(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
	u8 count;
    char *err;
    if(2 != argc)
	{
		ENG_LOG("both primary & center channel should be set\n");
        return -1;
	}
    *((unsigned int *)s_buf) =   strtol(argv[0], &err, 10);
	*((unsigned int *)(s_buf+1)) =   strtol(argv[1], &err, 10);
	for(count = 0; count < (sizeof(g_chan_list)/sizeof(struct chan_t)); count++)
	{
		if ((*s_buf == g_chan_list[count].primary_chan) &&
			*(s_buf + 1) == g_chan_list[count].center_chan)
			break;
	}
	if (count == sizeof(g_chan_list)/sizeof(struct chan_t))
	{
		 ENG_LOG(" primary or center channel is invalid\n");
		 return -1;
	}

    if(err == argv[0])
        return -1;
    *s_len = 2;
    return 0;
}

/*-----CMD ID:7-----------*/
int wlnpi_show_get_channel(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char primary20 = 0;
    unsigned char center = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(2 != r_len)
    {
        printf("get_channel err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
        return -1;
    }

    primary20 =  *( (unsigned char *)(r_buf+0) );
    center =  *((unsigned char *)(r_buf+1));
    printf("ret: primary_channel:%d,center_channel:%d :end\n", primary20, center);

    ENG_LOG("ADL leaving %s(), primary20 = %d, center=%d  return 0", __func__, primary20, center);

    return 0;
}

/*-----CMD ID:8-----------*/
int wlnpi_show_get_rssi(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    signed char rssi = 0;
    FILE *fp = NULL;
    char tmp[64] = {0x00};

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_rssi err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    rssi = *((signed char *)r_buf);
    printf("ret: %d:end\n", rssi );
    ENG_LOG("%s rssi is : %d \n",__func__, rssi);
    snprintf(tmp, 64, "rssi is : %d", rssi);

    if(NULL != (fp = fopen(IWNPI_EXEC_TMP_FILE, "w+"))) {
        int write_cnt = 0;

        write_cnt = fputs(tmp, fp);
        ENG_LOG("ADL %s(), callED fputs(%s), write_cnt = %d", __func__, tmp, write_cnt);
        fclose(fp);
    }
    else {
	ENG_LOG("%s open file error\n", __func__);
        return -1;
    }
    return 0;
}

/*-----CMD ID:9-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_tx_mode
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   para:0：duty cycle；1：carrier suppressioon；2：local leakage
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_tx_mode(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        ENG_LOG("ADL %s(), argc is ERROR, return -1", __func__);
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        ENG_LOG("ADL %s(), strtol is ERROR, return -1", __func__);
        return -1;
    }

	if(*s_buf > 2)
	{
		ENG_LOG("index value:%d exceed define value", *s_buf);
		return -1;
	}

    *s_len = 1;

    ENG_LOG("ADL leaving %s(), s_buf[0] = %d, s_len = %d", __func__, s_buf[0], *s_len);
    return 0;
}

/*-----CMD ID:10-----------*/
/********************************************************************
*   name   wlnpi_show_get_tx_mode
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_tx_mode(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char tx_mode = 0;
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_tx_mode err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
    }

    tx_mode = *( (unsigned char *)(r_buf+0) ) ;
	if(tx_mode > 2)
	{
		ENG_LOG("index value:%d exceed define value", tx_mode);
		return -1;
	}
    printf("ret: %d :end\n", tx_mode);

    ENG_LOG("ADL leaving %s(), tx_mode = %d, return 0", __func__, tx_mode);

	return 0;
}

/*-----CMD ID:11-----------*/
int wlnpi_cmd_set_rate(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);

	if(1 != argc)
        return -1;

    *((unsigned int *)s_buf) =   strtol(argv[0], &err, 10);
    if(err == argv[0])
        return -1;

	/*rate index should between 0 and 50*/
	if(*s_buf > 50)
	{
		ENG_LOG("rate index value:%d exceed design value\n", *s_buf);
		return -1;
	}

    *s_len = 1;

    ENG_LOG("ADL leaving %s(), s_buf[0] = %d, s_len = %d", __func__, s_buf[0], *s_len);
    return 0;
}

/*-----CMD ID:12-----------*/
int wlnpi_show_get_rate(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char rate = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_rate err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    rate = *( (unsigned char *)r_buf );
    printf("ret: %d :end\n", rate);

    ENG_LOG("ADL leaving %s(), rate = %d, return 0", __func__, rate);

    return 0;
}

/*-----CMD ID:13-----------*/
/******************************************************************
*   name   wlnpi_cmd_set_band
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_band(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        return -1;
    }

    *s_len = 1;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:14-----------*/
/********************************************************************
*   name   wlnpi_show_get_band
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_band(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int band = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_band err \n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    band = *( (unsigned char *)(r_buf+0) ) ;
    printf("ret: %d :end\n", band);

    ENG_LOG("ADL leaving %s(), band = %d, return 0", __func__, band);

    return 0;
}

/*-----CMD ID:15-----------*/
/******************************************************************
*   name   wlnpi_cmd_set_cbw
*   ---------------------------
*   description: set channel bandwidth
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_cbw(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        return -1;
    }

	if(*s_buf > 4)
	{
		ENG_LOG("index value:%d exceed define", *s_buf);
	}

    *s_len = 1;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:16-----------*/
/********************************************************************
*   name   wlnpi_show_get_bandwidth
*   ---------------------------
*   description: get channel bandwidth
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_cbw(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int bandwidth = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_bandwidth err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    bandwidth = *( (unsigned char *)(r_buf+0) );
    printf("ret: %d :end\n", bandwidth);

    ENG_LOG("ADL leaving %s(), bandwidth = %d, return 0", __func__, bandwidth);

    return 0;
}

/*-----CMD ID:17-----------*/
/******************************************************************
*   name   wlnpi_cmd_set_pkt_length
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_pkt_length(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);

    if(1 != argc)
    {
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    *s_len = 2;
    if(err == argv[0])
    {
        return -1;
    }

    ENG_LOG("ADL leaving %s()", __func__);

    return 0;
}

/*-----CMD ID:18-----------*/
/********************************************************************
*   name   wlnpi_show_get_pkt_length
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_pkt_length(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned short pktlen = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(2 != r_len)
    {
        printf("get_pkt_length err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    pktlen = *( (unsigned short *)(r_buf+0) ) ;
    printf("ret: %d :end\n", pktlen);

    ENG_LOG("ADL leaving %s(), pktlen = %d, return 0", __func__, pktlen);

    return 0;
}

/*-----CMD ID:19-----------*/
/*******************************************************************
*   name   wlnpi_cmd_set_preamble
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_preamble(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        return -1;
    }
	if(*s_buf > 4)
	{
		 ENG_LOG("index value:%d exceed define!", *s_buf);
		 return -1;
	}

    *s_len = 1;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:21-----------*/
/******************************************************************
*   name   wlnpi_cmd_set_guard_interval
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_guard_interval(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        ENG_LOG("ADL %s(), argc is ERROR, return -1", __func__);
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        ENG_LOG("ADL %s(), strtol is ERROR, return -1", __func__);
        return -1;
    }
	if(*s_buf >1)
	{
		ENG_LOG("gi index:%d not equal 0 or 1", *s_buf);
		return -1;
	}

    *s_len = 1;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:21-----------*/
/********************************************************************
*   name   wlnpi_show_get_guard_interval
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_guard_interval(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char gi = 0;
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_payload err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    gi = *( (unsigned char *)(r_buf+0) ) ;
    printf("ret: %d :end\n", gi);

    ENG_LOG("ADL leaving %s(), gi = %d, return 0", __func__, gi);

    return 0;
}

/*-----CMD ID:24-----------*/
/*******************************************************************
*   name   wlnpi_cmd_set_payload
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_payload(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
    if(1 != argc)
    {
        return -1;
    }

    *((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
    if(err == argv[0])
    {
        return -1;
    }

    *s_len = 1;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:25-----------*/
/********************************************************************
*   name   wlnpi_show_get_payload
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_payload(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int result = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_payload err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    result = *( (unsigned char *)(r_buf+0) ) ;
    printf("ret: %d :end\n", result);

    ENG_LOG("ADL leaving %s(), result = %d, return 0", __func__, result);

    return 0;
}

/*-----CMD ID:26-----------*/
int wlnpi_cmd_set_tx_power(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;
    if(1 != argc)
        return -1;
    *((unsigned int *)s_buf) =   strtol(argv[0], &err, 10);
    if(err == argv[0])
        return -1;
    *s_len = 1;
    return 0;
}

/*-----CMD ID:27-----------*/
int wlnpi_show_get_tx_power(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char TSSI;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get_tx_power err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    TSSI = r_buf[0];
    printf("ret: %d :end\n", TSSI);

    ENG_LOG("ADL leaving %s(), TSSI:%d return 0", __func__, TSSI);

    return 0;
}

/*-----CMD ID:28-----------*/
int wlnpi_cmd_set_tx_count(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;
    if(1 != argc)
        return -1;
    *s_buf =   strtol(argv[0], &err, 10);
    *s_len = 4;
    if(err == argv[0])
        return -1;
    return 0;
}

/*-----CMD ID:29-----------*/
int wlnpi_show_get_rx_ok(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    FILE *fp = NULL;
    char ret_buf[WLNPI_RESULT_BUF_LEN] = {0x00};
    unsigned int rx_end_count = 0;
    unsigned int rx_err_end_count = 0;
    unsigned int fcs_faiil_count = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(12 != r_len)
    {
        printf("get_rx_ok err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    rx_end_count = *((unsigned int *)(r_buf+0));
    rx_err_end_count = *((unsigned int *)(r_buf+4));
    fcs_faiil_count = *((unsigned int *)(r_buf+8));

    snprintf(ret_buf, WLNPI_RESULT_BUF_LEN, "ret: reg value: rx_end_count=%d rx_err_end_count=%d fcs_fail_count=%d :end\n", rx_end_count, rx_err_end_count, fcs_faiil_count);

    if(NULL != (fp = fopen(IWNPI_EXEC_TMP_FILE, "w+")))
    {
        int write_cnt = 0;

        write_cnt = fputs(ret_buf, fp);
        ENG_LOG("ADL %s(), callED fputs(%s), write_cnt = %d", __func__, ret_buf, write_cnt);

        fclose(fp);
    }

    printf("%s", ret_buf);
    ENG_LOG("ADL leaving %s(), rx_end_count=%d rx_err_end_count=%d fcs_fail_count=%d, return 0", __func__, rx_end_count, rx_err_end_count, fcs_faiil_count);

    return 0;
}

/*-----CMD ID:34-----------*/
int wlnpi_cmd_get_reg(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    unsigned char  *type  = s_buf;
    unsigned int   *addr  = (unsigned int *)(s_buf + 1);
    unsigned int   *count = (unsigned int *)(s_buf + 5);
    char *err;
    if((argc < 2)|| (argc > 3) || (!argv) )
        return -1;
    if(0 == strncmp(argv[0], "mac", 3) )
    {
        *type = 0;
    }
    else if(0 == strncmp(argv[0], "phy0", 4) )
    {
        *type = 1;
    }
    else if(0 == strncmp(argv[0], "phy1", 4) )
    {
        *type = 2;
    }
    else if(0 == strncmp(argv[0], "rf",  2) )
    {
        *type = 3;
    }
    else
    {
        return -1;
    }

    *addr =   strtol(argv[1], &err, 16);
    if(err == argv[1])
    {
        return -1;
    }

    ENG_LOG("ADL %s(), argv[2] addr = %p", __func__, argv[2]);
    if (NULL == argv[2] || 0 == strlen(argv[2]))
    {
        /* if argv[2] is NULL or null string, set count to default value, which value is 1 */
        ENG_LOG("ADL %s(), argv[2] is null, set count to 1", __func__);
        *count = 1;
    }
    else
    {
        *count =  strtol(argv[2], &err, 10);
        if(err == argv[2])
        {
            /* if exec strtol function is error, set count to default value, which value is 1 */
            ENG_LOG("ADL %s(), exec strtol(argv[2]) is error, set count to 1", __func__);
            *count = 1;
        }
    }

    if (*count >= 5)
    {
        ENG_LOG("ADL %s(), *count is too large, *count = %d, set count to 5", __func__, *count);
        *count = 5;
    }
    ENG_LOG("ADL %s(), *count = %d", __func__, *count);

    *s_len = 9;
    return 0;
}

/*-----CMD ID:34-----------*/
#define WLNPI_GET_REG_MAX_COUNT            (20)
int wlnpi_show_get_reg(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    int i, ret, p;
    FILE *fp = NULL;
    char str[256] = {0};
    char ret_buf[WLNPI_RESULT_BUF_LEN] = {0x00};

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    for(i=0, p =0; (i < r_len/4) && (i < WLNPI_GET_REG_MAX_COUNT); i++)
    {
	if (!(i % 4) && i) {
	    ret = sprintf((str +  p), "\n");
	    p = p + ret;
	}
	ret = sprintf((str +  p),  "0x%08X\t",  *((int *)(r_buf + i*4)));
	p = p + ret;
    }

    snprintf(ret_buf, WLNPI_RESULT_BUF_LEN, "reg values is :%s\n", str);

    if(NULL != (fp = fopen(IWNPI_EXEC_TMP_FILE, "w+")))
    {
        int write_cnt = 0;

        write_cnt = fputs(ret_buf, fp);
        ENG_LOG("ADL %s(), callED fputs(%s), write_cnt = %d", __func__, ret_buf, write_cnt);

        fclose(fp);
    }

    printf("%s", ret_buf);

    ENG_LOG("ADL leaving %s(), str = %s, return 0", __func__, str);

    return 0;
}

/*-----CMD ID:35-----------*/
int wlnpi_cmd_set_reg(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    unsigned char  *type  = s_buf;
    unsigned int   *addr  = (unsigned int *)(s_buf + 1);
    unsigned int   *value = (unsigned int *)(s_buf + 5);
    char *err;
    if((argc < 2)|| (argc > 3) || (!argv) )
        return -1;
    if(0 == strncmp(argv[0], "mac", 3) )
    {
        *type = 0;
    }
    else if(0 == strncmp(argv[0], "phy0", 4) )
    {
        *type = 1;
    }
    else if(0 == strncmp(argv[0], "phy1", 4) )
    {
        *type = 2;
    }
    else if(0 == strncmp(argv[0], "rf",  2) )
    {
        *type = 3;
    }
    else
    {
        return -1;
    }
    *addr =   strtol(argv[1], &err, 16);
    if(err == argv[1])
        return -1;

    *value =  strtoul(argv[2], &err, 16);
    if(err == argv[2])
        return -1;
    *s_len = 9;
    return 0;
}

/*-----CMD ID:39-----------*/
int wlnpi_show_get_lna_status(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    FILE *fp = NULL;
    char ret_buf[WLNPI_RESULT_BUF_LEN+1] = {0x00};
    unsigned char status = 0;

    ENG_LOG("ADL entry %s(), r_eln = %d", __func__, r_len);
    status = r_buf[0];

    snprintf(ret_buf, WLNPI_RESULT_BUF_LEN, "ret: %d :end\n", status);
    printf("%s", ret_buf);

    if(NULL != (fp = fopen(IWNPI_EXEC_TMP_FILE, "w+")))
    {
        int write_cnt = 0;

        write_cnt = fputs(ret_buf, fp);
        ENG_LOG("ADL %s(), write_cnt = %d ret_buf %s", __func__, write_cnt, ret_buf);

        fclose(fp);
    }

    ENG_LOG("ADL leaving %s(), status = %d", __func__, status);
    return 0;
}

/*-----CMD ID:40-----------*/
int wlnpi_cmd_set_wlan_cap(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;
    if(1 != argc)
        return -1;
    *(unsigned int *)s_buf = strtol(argv[0], &err, 10);
    *s_len = 4;
    if(err == argv[0])
        return -1;
    return 0;
}

/*-----CMD ID:41-----------*/
int wlnpi_show_get_wlan_cap(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int cap = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(4 != r_len)
    {
        printf("get_rssi err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    cap = *(unsigned int *)r_buf;
    printf("ret: %d:end\n", cap );

    ENG_LOG("ADL leaving %s(), cap = %d, return 0", __func__, cap);

    return 0;
}
int wlnpi_cmd_get_reconnect(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
    *s_buf = (unsigned char)GET_RECONNECT;
    *s_len = 1;
    ENG_LOG("ADL entry %s(), subtype = %d, len = %d\n", __func__, *s_buf, *s_len);
    return 0;
}

/*-----CMD ID:42-----------*/
int wlnpi_show_reconnect(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int retcnt = 0;
    FILE *fp = NULL;
    char tmp[64] = {0x00};
    retcnt = *((unsigned int *)(r_buf));

    snprintf(tmp, 64, "reconnect : %d", retcnt);
    ENG_LOG("%s tmp is:%s reconnect result : %d, r_len : %d\n",__func__, tmp, retcnt, r_len);

    if(NULL != (fp = fopen(IWNPI_EXEC_TMP_FILE, "w+"))) {
        int write_cnt = 0;

	write_cnt = fputs(tmp, fp);
        ENG_LOG("ADL %s(), callED fputs(%s) write_cnt = %d", __func__, tmp, write_cnt);
        fclose(fp);
    }else {
	ENG_LOG("%s open file error\n",__func__);
    }

    return 0;
}
/*-----CMD ID:42-----------*/
/********************************************************************
*   name   wlnpi_show_get_connected_ap_info
*   ---------------------------
*   description: get connected ap info
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_connected_ap_info(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    assoc_resp resp_ies = {0x00};

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    //iwnpi_hex_dump((unsigned char *)"r_buf:\n", strlen("r_buf:\n"), (unsigned char *)r_buf, r_len);

    if (NULL != r_buf)
    {
        memcpy(&resp_ies, r_buf, r_len);
    }
    else
    {
        ENG_LOG("ADL levaling %s(), resp_ies == NULL!!!", __func__);
        return -1;
    }

    if (!resp_ies.connect_status)
    {
        printf("not connected AP.\n");
        ENG_LOG("ADL levaling %s(), connect_status = %d", __func__, resp_ies.connect_status);

        return 0;
    }

    printf("Current Connected Ap info: \n");

    /* SSID */
    printf("SSID: %s\n", resp_ies.ssid);

    /* connect mode */
    printf("Mode: %s\t", (resp_ies.conn_mode ? IWNPI_CONN_MODE_OTHER : IWNPI_CONN_MODE_MANAGED));

    /* RSSI */
    printf("RSSI: %d dBm\t", resp_ies.rssi);

    /* SNR */
    printf("SNR: %d dB\t", resp_ies.snr);

    /* noise */
    printf("noise: %d dBm\n", resp_ies.noise);

    /* Flags: FromBcn RSSI on-channel Channel */
    printf("Flags: FromBcn RSSI on-channel Channel: %d\n", resp_ies.channel);

    /* BSSID */
    printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n", resp_ies.bssid[0], resp_ies.bssid[1], resp_ies.bssid[2], resp_ies.bssid[3], resp_ies.bssid[4], resp_ies.bssid[5]);

    /* capability */
    wlnpi_ap_info_print_capa(resp_ies.assoc_resp_info);

    /* Support Rates */
    wlnpi_ap_info_print_support_rate(resp_ies.assoc_resp_info);

    /* HT Capable: */
    {
        printf("\n");
        printf("HT Capable:\n");
        printf("Chanspec: %s\t", IWNPI_DEFAULT_CHANSPEC);
        printf("Channel: %d Primary channel: %d\t", resp_ies.channel, resp_ies.channel);
        printf("BandWidth: %s\n", IWNPI_DEFAULT_BANDWIDTH);

        /* HT Capabilities */
        {
            wlnpi_ap_info_print_ht_capa(resp_ies.assoc_resp_info);
            wlnpi_ap_info_print_ht_mcs(resp_ies.assoc_resp_info);
        }
    }

    /* wps */
    wlnpi_ap_info_print_wps(resp_ies.assoc_resp_info);

    printf("\n");

    ENG_LOG("ADL leaving %s(), return 0", __func__);

    return 0;
}

/********************************************************************
*   name   wlnpi_ap_info_print_capa
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int wlnpi_ap_info_print_capa(const char *data)
{
    short capability = data[2];

    ENG_LOG("ADL entry %s()", __func__);

    capability = iwnpi_get_le16(&data[2]);
    ENG_LOG("ADL %s(), capability = 0x%x", __func__, capability);

    printf("\n");
    printf("Capability:\n");
    if (capability & 1)
    {
        printf("ESS Type Network.\t");
    }
    else if (capability >> 1 & 1)
    {
        printf("IBSS Type Network.\t");
    }

    if (capability >> 2 & 1)
    {
        printf("CF Pollable.\t");
    }

    if (capability >> 3 & 1)
    {
        printf("CF Poll Requested.\t");
    }

    if (capability >> 4 & 1)
    {
        printf("Privacy Enabled.\t");
    }

    if (capability >> 5 & 1)
    {
        printf("Short Preamble.\t");
    }

    if (capability >> 6 & 1)
    {
        printf("PBCC Allowed.\t");
    }

    if (capability >> 7 & 1)
    {
        printf("Channel Agility Used.\t");
    }

    if (capability >> 8 & 1)
    {
        printf("Spectrum Mgmt Enabled.\t");
    }

    if (capability >> 9 & 1)
    {
        printf("QoS Supported.\t");
    }

    if (capability >> 10 & 1)
    {
        printf("G Mode Short Slot Time.\t");
    }

    if (capability >> 11 & 1)
    {
        printf("APSD Supported.\t");
    }

    if (capability >> 12 & 1)
    {
        printf("Radio Measurement.\t");
    }

    if (capability >> 13 & 1)
    {
        printf("DSSS-OFDM Allowed.\t");
    }

    if (capability >> 14 & 1)
    {
        printf("Delayed Block Ack Allowed.\t");
    }

    if (capability >> 15 & 1)
    {
        printf("Immediate Block Ack Allowed.\t");
    }

    printf("\n");
    ENG_LOG("ADL levaling %s()", __func__);
    return 0;
}

/********************************************************************
*   name   wlnpi_ap_info_print_wps
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int wlnpi_ap_info_print_wps(const char *data)
{
    char    *vendor_ie = NULL;
    char    wps_version = 0;
    char    wifi_protected_setup = 0;

    ENG_LOG("ADL entry %s()", __func__);

    vendor_ie = iwnpi_bss_get_vendor_ie(data, WPS_IE_VENDOR_TYPE);
    if (NULL == vendor_ie)
    {
        ENG_LOG("ADL %s(), get vendor failed. return 0", __func__);
        return 0;
    }

    //iwnpi_hex_dump("vendor:", strlen("vendor:"), (unsigned char *)vendor_ie, 0x16);

    ENG_LOG("ADL %s(), length = %d\n", __func__, vendor_ie[1]);
    wps_version = vendor_ie[10];
    wifi_protected_setup = vendor_ie[15];

    printf("\nWPS:\t");
    printf("0x%02x \t", wps_version);

    if (2 == wifi_protected_setup)
    {
        printf("Configured.");
    }
    else if (3 == wifi_protected_setup)
    {
        printf("AP.");
    }

    printf("\n");

    ENG_LOG("ADL levaling %s()", __func__);
    return 0;
}

/********************************************************************
*   name   wlnpi_ap_info_print_ht_mcs
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int wlnpi_ap_info_print_ht_mcs(const char *data)
{
    char    *ht_capa_ie = NULL;
    char    spatial_stream1 = 0;
    char    spatial_stream2 = 0;
    char    spatial_stream3 = 0;
    char    spatial_stream4 = 0;

    ht_capa_ie = wlnpi_bss_get_ie(data, WLAN_EID_HT_CAP);
    if (NULL == ht_capa_ie)
    {
        printf("error. get mcs failed.\n");
        ENG_LOG("ADL %s(), get mcs failed. return -1", __func__);

        return -1;
    }

    //iwnpi_hex_dump("ht_capability:", strlen("ht_capability:"), (unsigned char *)ht_capa_ie, 0x16);

    spatial_stream1 = ht_capa_ie[5];
    spatial_stream2 = ht_capa_ie[6];
    spatial_stream3 = ht_capa_ie[7];
    spatial_stream4 = ht_capa_ie[8];

    //printf("spatial1 = 0x%x, spatial2 = 0x%x\n", spatial_stream1, spatial_stream2);

    printf("\nSupported MCS:\n");

    if (spatial_stream1 >> 0 & 1)
    {
        printf("0 ");
    }

    if (spatial_stream1 >> 1 & 1)
    {
        printf("1 ");
    }

    if (spatial_stream1 >> 2 & 1)
    {
        printf("2 ");
    }

    if (spatial_stream1 >> 3 & 1)
    {
        printf("3 ");
    }

    if (spatial_stream1 >> 4 & 1)
    {
        printf("4 ");
    }

    if (spatial_stream1 >> 5 & 1)
    {
        printf("5 ");
    }

    if (spatial_stream1 >> 6 & 1)
    {
        printf("6 ");
    }

    if (spatial_stream1 >> 7 & 1)
    {
        printf("7");
    }

    /* spatial2 */
    if (spatial_stream2 >> 0 & 1)
    {
        printf("8 ");
    }

    if (spatial_stream2 >> 1 & 1)
    {
        printf("9 ");
    }

    if (spatial_stream2 >> 2 & 1)
    {
        printf("10 ");
    }

    if (spatial_stream2 >> 3 & 1)
    {
        printf("11 ");
    }

    if (spatial_stream2 >> 4 & 1)
    {
        printf("12 ");
    }

    if (spatial_stream2 >> 5 & 1)
    {
        printf("13 ");
    }

    if (spatial_stream2 >> 6 & 1)
    {
        printf("14 ");
    }

    if (spatial_stream2 >> 7 & 1)
    {
        printf("15");
    }

    /* spatial3 */
    if (spatial_stream3 >> 0 & 1)
    {
        printf("16 ");
    }

    if (spatial_stream3 >> 1 & 1)
    {
        printf("17 ");
    }

    if (spatial_stream3 >> 2 & 1)
    {
        printf("18 ");
    }

    if (spatial_stream3 >> 3 & 1)
    {
        printf("19 ");
    }

    if (spatial_stream3 >> 4 & 1)
    {
        printf("20 ");
    }

    if (spatial_stream3 >> 5 & 1)
    {
        printf("21 ");
    }

    if (spatial_stream3 >> 6 & 1)
    {
        printf("22 ");
    }

    if (spatial_stream3 >> 7 & 1)
    {
        printf("23");
    }

    /* spatial4 */
    if (spatial_stream4 >> 0 & 1)
    {
        printf("24 ");
    }

    if (spatial_stream4 >> 1 & 1)
    {
        printf("25 ");
    }

    if (spatial_stream4 >> 2 & 1)
    {
        printf("26 ");
    }

    if (spatial_stream4 >> 3 & 1)
    {
        printf("27 ");
    }

    if (spatial_stream4 >> 4 & 1)
    {
        printf("28 ");
    }

    if (spatial_stream4 >> 5 & 1)
    {
        printf("29 ");
    }

    if (spatial_stream4 >> 6 & 1)
    {
        printf("30 ");
    }

    if (spatial_stream4 >> 7 & 1)
    {
        printf("31 ");
    }

    printf("\n");

    return 0;
}

/********************************************************************
*   name   wlnpi_ap_info_print_ht_capa
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int wlnpi_ap_info_print_ht_capa(const char *data)
{
    char    *ht_capa_ie = NULL;
    short   ht_capa = 0;

    ENG_LOG("ADL entry %s()", __func__);

    ht_capa_ie = wlnpi_bss_get_ie(data, WLAN_EID_HT_CAP);
    if (NULL == ht_capa_ie)
    {
        printf("error. get support_rate failed.\n");
        ENG_LOG("ADL %s(), get support_rate failed. return -1", __func__);

        return -1;
    }

    //iwnpi_hex_dump("ht_capability:", strlen("ht_capability:"), (unsigned char *)ht_capa_ie, 16);

    ENG_LOG("ADL %s(), len = %d\n", __func__, ht_capa_ie[1]);

    ht_capa = iwnpi_get_le16(&ht_capa_ie[2]);

    ENG_LOG("ADL %s(), HT Capabilities = 0x%x\n", __func__, ht_capa);

    printf("\n");
    printf("HT Capabilities:\n");

    if (ht_capa >> 0 & 1)
    {
        printf("LDPC Coding Capability.\n");
    }

    if (ht_capa >> 1 & 1)
    {
        printf("20MHz and 40MHz Operation is Supported.\n");
    }

    if (ht_capa >> 2 & 3)
    {
        printf("Spatial Multiplexing Enabled.\n");
    }

    if (ht_capa >> 4 & 1)
    {
        printf("Can receive PPDUs with HT-Greenfield format.\n");
    }

    if (ht_capa >> 5 & 1)
    {
        printf("Short GI for 20MHz.\n");
    }

    if (ht_capa >> 6 & 1)
    {
        printf("Short GI for 40MHz.\n");
    }

    if (ht_capa >> 7 & 1)
    {
        printf("Transmitter Support Tx STBC.\n");
    }

    if (ht_capa >> 8 & 3)/*2 bit*/
    {
        printf("Rx STBC Support.\n");
    }

    if (ht_capa >> 10 & 1)
    {
        printf("Support HT-Delayed BlockAck Operation.\n");
    }

    if (ht_capa >> 11 & 1)
    {
        printf("Maximal A-MSDU size.\n");
    }

    if (ht_capa >> 12 & 1)
    {
        printf("BSS Allow use of DSSS/CCK Rates @40MHz\n");
    }

    if (ht_capa >> 13 & 1)
    {
        printf("Device/BSS Support use of PSMP.\n");
    }

    if (ht_capa >> 14 & 1)
    {
        printf("AP allow use of 40MHz Transmissions In Neighboring BSSs.\n");
    }

    if (ht_capa >> 15 & 1)
    {
        printf("L-SIG TXOP Protection Support.\n");
    }

    ENG_LOG("ADL leval %s()", __func__);
    return 0;
}

/********************************************************************
*   name   wlnpi_ap_info_print_support_rate
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static int wlnpi_ap_info_print_support_rate(const char *data)
{
    char i = 0;
    char length = 0;
    char *support_rate_ie = NULL;

    ENG_LOG("ADL entry %s()", __func__);

    //iwnpi_hex_dump("data:", strlen("data:"), (unsigned char *)data, 100);
    support_rate_ie = wlnpi_bss_get_ie(data, WLAN_EID_SUPP_RATES);
    if (NULL == support_rate_ie)
    {
        printf("error. get support_rate failed.\n");
        ENG_LOG("ADL %s(), get support_rate failed. return -1", __func__);

        return -1;
    }

    //iwnpi_hex_dump("support rate:", strlen("support rate:"), (unsigned char *)support_rate_ie, 10);

    length = support_rate_ie[1];
    ENG_LOG("ADL %s(), length = %d\n", __func__, length);

    printf("\n");
    printf("Support Rates:\n");

    while (i < length)
    {
        printf("%s  ", wlnpi_get_rate_by_phy(support_rate_ie[2 + i]));
        i++;
    }

    printf("\n");

    ENG_LOG("ADL levaling %s()", __func__);

    return 0;
}

/********************************************************************
*   name   wlnpi_get_rate_by_phy
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static char *wlnpi_get_rate_by_phy(int phy)
{
    unsigned char i = 0;
    char id_num = sizeof(g_rate_table) / sizeof(iwnpi_rate_table);

    for (i = 0; i < id_num; i++)
    {
        if (phy == g_rate_table[i].phy_rate)
        {
            ENG_LOG("\nADL %s(), rate: phy = 0x%2x, str = %s\n", __func__, phy, g_rate_table[i].str_rate);
            return g_rate_table[i].str_rate;
        }
    }

    ENG_LOG("ADL %s(), not match rate, phy = 0x%x", __func__, phy);
    return "";
}

/********************************************************************
*   name   wlnpi_bss_get_ie
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static char *wlnpi_bss_get_ie(const char *bss, char ieid)
{
    short ie_len = 0;
    const char *end, *pos;

    /* get length of ies */
    ie_len = bss[0] + bss[1];
    ENG_LOG("%s(), ie_len = %d\n", __func__, ie_len);

    pos = (const char *) (bss + 2 + 6);/* 6 is capability's length + status code length + AID length */
    end = pos + ie_len - 6;

    while (pos + 1 < end)
    {
        if (pos + 2 + pos[1] > end)
        {
            break;
        }

        if (pos[0] == ieid)
        {
            return (char *)pos;
        }

        pos += 2 + pos[1];
    }

    return NULL;
}

/********************************************************************
*   name   iwnpi_bss_get_vendor_ie
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
static char *iwnpi_bss_get_vendor_ie(const char *bss, int vendor_type)
{
    short ie_len = 0;
    const char *end, *pos;

    /* get length of ies */
    ie_len = bss[0] + bss[1];

    pos = (const char *)(bss + 2+6);
    end = pos + ie_len - 6;

    while (pos + 1 < end)
    {
        if (pos + 2 + pos[1] > end)
        {
            break;
        }

        if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 && vendor_type == iwnpi_get_be32(&pos[2]))
        {
            return (char *)pos;
        }
        pos += 2 + pos[1];
    }

    return NULL;
}

/*-----CMD ID:43-----------*/
/********************************************************************
*   name   wlnpi_show_get_mcs_index
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_mcs_index(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int mcs_index = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(1 != r_len)
    {
        printf("get msc index err \n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    mcs_index = *( (unsigned char *)(r_buf+0) ) ;
    printf("mcs index: %d\n", mcs_index);

    ENG_LOG("ADL leaving %s(), mcs_index = %d, return 0", __func__, mcs_index);

    return 0;
}

/*-----CMD ID:44-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_ar
*   ---------------------------
*   description: set autorate's flag and index
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_ar(int argc, char **argv, unsigned char *s_buf, int *s_len )
{
    char *endptr = NULL;
    unsigned char   *ar_flag  = (unsigned char *)(s_buf + 0);
    unsigned char   *ar_index = (unsigned char *)(s_buf + 1);
    unsigned char   *ar_sgi   = (unsigned char *)(s_buf + 2);

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

    if((argc < 1)|| (argc > 4) || (!argv))
    {
        ENG_LOG("ADL leaving %s(), argc ERR.", __func__);
        return -1;
    }

    *ar_flag = strtoul(argv[0], &endptr, 10);
    if(*endptr != '\0')
    {
        ENG_LOG("ADL leaving %s(), strtol(ar_flag) ERR.", __func__);
        return -1;
    }

    if (1 == *ar_flag)
    {
        *ar_index = strtoul(argv[1], &endptr, 10);
        if(*endptr != '\0')
        {
            ENG_LOG("ADL leaving %s(), strtol(ar_index) ERR.", __func__);
            return -1;
        }

        *ar_sgi = strtoul(argv[2], &endptr, 10);
        if(*endptr != '\0')
        {
            ENG_LOG("ADL leaving %s(), strtol(ar_sgi) ERR.", __func__);
            return -1;
        }
    }
    else if (0 == *ar_flag)
    {
        *ar_index = 0;
        *ar_sgi = 0;
    }
    else
    {
        ENG_LOG("ADL leaving %s(), ar_flag is invalid. *ar_flag = %d", __func__, *ar_flag);
        return -1;
    }

    *s_len = 3;

    ENG_LOG("ADL leaving %s(), ar_flag = %d, ar_index = %d, *s_len = %d", __func__, *ar_flag, *ar_index, *s_len);

    return 0;
}


/*-----CMD ID:45-----------*/
/********************************************************************
*   name   wlnpi_show_get_ar
*   ---------------------------
*   description: set autorate's flag and autorate index
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_ar(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned char ar_flag = 0;
    unsigned char ar_index = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    ar_flag  = *(unsigned char *)(r_buf + 0);
    ar_index = *(unsigned char *)(r_buf + 1);

    printf("ret: %d, %d :end\n", ar_flag, ar_index);

    ENG_LOG("ADL leaving %s(), ar_flag = %d, ar_index= %d, return 0", __func__, ar_flag, ar_index);

    return 0;
}

/*-----CMD ID:46-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_ar_pktcnt
*   ---------------------------
*   description: set autorate's pktcnt
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_ar_pktcnt(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

    if(1 != argc)
    {
        ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
        return -1;
    }

    *((unsigned int *)s_buf) = strtoul(argv[0], &err, 10);
    if(err == argv[0])
    {
        ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
        return -1;
    }

    *s_len = 4;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:47-----------*/
/********************************************************************
*   name   wlnpi_show_get_ar_pktcnt
*   ---------------------------
*   description: get autorate's pktcnt
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_ar_pktcnt(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int pktcnt = 0;
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(4 != r_len)
    {
        printf("get_ar_pktcnt err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
        return -1;
    }

    pktcnt = *((unsigned int *)(r_buf+0));
    printf("ret: %d :end\n", pktcnt);

    ENG_LOG("ADL leaving %s(), pktcnt = %d, return 0", __func__, pktcnt);

    return 0;
}

/*-----CMD ID:48-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_ar_retcnt
*   ---------------------------
*   description: set autorate's retcnt
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_ar_retcnt(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char *err;

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

    if(1 != argc)
    {
        ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
        return -1;
    }

    *((unsigned int *)s_buf) = strtoul(argv[0], &err, 10);
    if(err == argv[0])
    {
        ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
        return -1;
    }

    *s_len = 4;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:49-----------*/
/********************************************************************
*   name   wlnpi_show_get_ar_retcnt
*   ---------------------------
*   description: get autorate's retcnt
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_ar_retcnt(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int retcnt = 0;
    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(4 != r_len)
    {
        printf("get_ar_retcnt err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
        return -1;
    }

    retcnt = *((unsigned int *)(r_buf+0));
    printf("ret: %d :end\n", retcnt);

    ENG_LOG("ADL leaving %s(), retcnt = %d, return 0", __func__, retcnt);

    return 0;
}

/*-----CMD ID:51-----------*/
int wlnpi_cmd_roam(int argc, char **argv,  unsigned char *s_buf, int *s_len)
{
    char *err;
    int index = 0;
    unsigned char *tmp = s_buf;
	int length;

    if(3 != argc)
        return -1;

    *tmp = (unsigned char)strtol(argv[0], &err, 10);
    if(err == argv[0])
        return -1;

    *s_len = 1;
    tmp = tmp + 1;

    mac_addr_a2n(tmp, argv[1]);
    *s_len += 6;
    tmp = tmp + 6;

    length = strlen(argv[2]) + 1;
    strncpy((char *)tmp, argv[2], length);
    *s_len += length;

    ENG_LOG("roam  channel:%d mac:%02x:%02x:%02x:%02x:%02x:%02x ssid:%s\n",s_buf[0],
	    s_buf[1],s_buf[2],s_buf[3],s_buf[4],s_buf[5],s_buf[6],s_buf +7);

    return 0;
}

/*-----CMD ID:51-----------*/
int wlnpi_cmd_set_wmm(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
    char *err;
    unsigned char *tmp = s_buf;
    int type, cwmin, cwmax, aifs, txop;

    if(5 != argc) {
	ENG_LOG("Error ! para num is : %d, required 5\n", argc);
	return -1;
    }

    /*set be/bk/vi/vo */
    if (strcmp(argv[0], "be") == 0) {
	ENG_LOG("it is be \n");
	type = 0;
    } else if(strcmp(argv[0], "bk") ==0) {
	ENG_LOG("it is bk \n");
	type = 1;
    } else if(strcmp(argv[0], "vi") ==0) {
	ENG_LOG("it is vi \n");
	type = 2;
    } else if(strcmp(argv[0], "vo") ==0) {
	ENG_LOG("it is vo \n");
	type = 3;
    } else {
	ENG_LOG("invalid para : %s \n", argv[0]);
	return -1;
    }
    *tmp = type;
    *s_len = 1;
    tmp += 1;
    if (!is_digital(argv[1])) {
	ENG_LOG("Invalid cwmin para : %s\n", argv[1]);
	return -1;
    }

    /*set cwmin*/
    cwmin = strtol(argv[1], &err, 10);
    if (err == argv[1]) {
	ENG_LOG("get cwmin failed \n");
	return -1;
    }
    if (cwmin < 0 || cwmin > 255) {
	ENG_LOG("invalid cwmin value : %d \n", cwmin);
	return -1;
    }
    *tmp = cwmin;
    *s_len += 1;
    tmp += 1;

    /*set cwmax*/
    if (!is_digital(argv[2])) {
	ENG_LOG("Invalid cwmax para : %s\n",argv[2]);
	return -1;
    }
    cwmax = strtol(argv[2], &err, 10);
    if (err == argv[2]) {
	ENG_LOG("get cwmax failed \n");
	return -1;
    }
    if (cwmax < 0 || cwmax > 255) {
	ENG_LOG("invalid cwmax value : %d \n", cwmax);
	return -1;
    }
    *tmp = cwmax;
    *s_len += 1;
    tmp += 1;

    if (cwmin > cwmax) {
	ENG_LOG("cwmin larger than cwmax\n");
	return -1;
    }

    /*set aifs*/
    if (!is_digital(argv[3])) {
	ENG_LOG("Invalid aifs para : %s\n",argv[3]);
	return -1;
    }
    aifs = strtol(argv[3], &err, 10);
    if (err == argv[3]) {
	ENG_LOG("get aifs failed\n");
	return -1;
    }
    if (aifs < 0 || aifs > 255) {
	ENG_LOG("invalid aifs value : %d \n", aifs);
	return -1;
    }
    *tmp = aifs;
    *s_len += 1;
    tmp += 1;

    /*set txop*/
    if (!is_digital(argv[4])) {
	ENG_LOG("Invalid txop para : %s\n", argv[4]);
	return -1;
    }
    txop = strtol(argv[4], &err, 10);
    if (err == argv[4]) {
	ENG_LOG("get txop failed\n");
	return -1;
    }
    if (txop < 0 || txop > 65535) {
	ENG_LOG("invalid txop value : %d \n", txop);
	return -1;
    }
    *(unsigned short *)tmp = txop;
    *s_len += 2;

    printf("show para,len : %d \n", *s_len);
    printf("be|bk|vo|vi : %d \n", *s_buf);
    printf("cwmin : %d \n", *(s_buf + 1));
    printf("cwmax : %d \n", *(s_buf + 2));
    printf("aifs  : %d \n", *(s_buf + 3));
    printf("txop  : %d \n", *(unsigned short *)(s_buf + 4));

    return 0;
}

/*-----CMD ID:52-----------*/
int wlnpi_cmd_set_eng_mode(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
    int value;
    unsigned char *p = s_buf;
    char *err;

#define SET_CCA_STATE  1
#define GET_CCA_STATE  2
#define SET_SCAN_STATE 3
#define GET_SCAN_STATE 4

    ENG_LOG("ADL entry %s(), argc = %d , argv[0] = %s", __func__, argc, argv[0]);

    if((1 != argc && 2 != argc) || p == NULL)
        return -1;

	/* command id (1: Adaptive Mode) */
	value = strtoul(argv[0], &err, 10);
	if (err == argv[0])
	    return -1;

	*p++ = value;

	switch (value)
	{
		case SET_CCA_STATE:
		case SET_SCAN_STATE:
			/* data length */
			*p++ = 1;

			/* data (0: off, 1: on) */
			value = strtoul(argv[1], &err, 10);
			if (err == argv[1])
			    return -1;

			*p = value;
			*s_len = 3;
			break;

		default:
			*s_len = 1;
			break;
	}

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}

/*-----CMD ID:55-----------*/
/******************************************************************
*   name   wlnpi_cmd_start_pkt_log
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   u16 buffer_num, u16 duration(default : 0)
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_start_pkt_log(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    char **err = NULL;
	unsigned short *buffer_num = (unsigned short *)s_buf;
    unsigned short *duration = (unsigned short *)(s_buf + 2);

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

    if(2 != argc)
    {
		ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
		return -1;
    }

    *buffer_num = strtoul(argv[0], err, 10);
    if(err)
    {
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
    }

    *duration = strtoul(argv[1], err, 10);
    if(err)
    {
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
    }

    *s_len = 4;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}


/*-----CMD ID:66-----------*/
int wlnpi_cmd_get_pa_info(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	int ret;
	unsigned long int val;

	if ((argc != 1) || (s_buf == NULL))
	{
		ENG_LOG("ADL leaving %s(), argc = %d ERR\n", __func__, argc);
		return -1;
	}

	ret = str2digit(argv[0], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}

	if ((val&0x7) == 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}

	*s_buf = val & 0x7;
	*s_len = sizeof(*s_buf);

	ENG_LOG("ADL leaving %s(), type = 0x%x", __func__, *s_buf);

	return 0;
}


/*-----CMD ID:67-----------*/
int wlnpi_cmd_tx_status(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	int ret;
	unsigned long int val;
	unsigned char ctxt_id, get_or_clear;

	if ((argc != 2) || (s_buf == NULL))
	{
		ENG_LOG("ADL leaving %s(), argc = %d ERR\n", __func__, argc);
		return -1;
	}
	ret = str2digit(argv[0], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}
	*s_buf = ctxt_id = val & 0xff;

	ret = str2digit(argv[1], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[1]);
		return -1;
	}
	get_or_clear = val & 0xff;
	if (get_or_clear != 0 && get_or_clear != 1)
	{

		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[1]);
		return -1;
	}
	*(s_buf + 1) = get_or_clear;
	*s_len = 2;

	ENG_LOG("ADL leaving %s(), ctxt_id = %d, get_or_clear = %d", __func__, ctxt_id, get_or_clear);

	return 0;
}


/*-----CMD ID:68-----------*/
int wlnpi_cmd_rx_status(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	int ret;
	unsigned long int val;
	unsigned char ctxt_id, get_or_clear;

	if ((argc != 2) || (s_buf == NULL))
	{
		ENG_LOG("ADL leaving %s(), argc = %d ERR\n", __func__, argc);
		return -1;
	}

	ret = str2digit(argv[0], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}
	*s_buf = ctxt_id = val & 0xff;
	ret = str2digit(argv[1], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[1]);
		return -1;
	}
	get_or_clear = val & 0xff;
	if (get_or_clear != 0 && get_or_clear != 1)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[1]);
		return -1;
	}
	*(s_buf + 1) = get_or_clear;
	*s_len = 2;

	ENG_LOG("ADL leaving %s(), ctxt_id = %d, get_or_clear = %d", __func__, ctxt_id, get_or_clear);

	return 0;
}


/*-----CMD ID:69-----------*/
int wlnpi_cmd_get_sta_lut_status(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	int ret;
	unsigned long int val;

	if ((argc != 1) || (s_buf == NULL))
	{
		ENG_LOG("ADL leaving %s(), argc = %d ERR\n", __func__, argc);
		return -1;
	}

	ret = str2digit(argv[0], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}

	*s_buf = val & 0xff;
	*s_len = sizeof(*s_buf);

	ENG_LOG("ADL leaving %s(), ctxt_id = %d", __func__, *s_buf);

	return 0;
}

/*-----CMD ID:73-----------*/
int wlnpi_show_get_qmu_status(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	if (20 != r_len) {
		printf("get_qmu_status err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	int i = 0;
	int len = r_len/2;
	unsigned char *type_name[10] = {
					"TX_HIGHQ",
					"TX_MSDUQ",
					"TX_INTF_LBUF",
					"TX_INTF_CBUF",
					"RX_HIGHQ",
					"RX_FILTERQ",
					"RX_NORMALQ",
					"RX_OFFLOADQ",
					"RX_INTF_LBUF",
					"RX_INTF_CBUF"
					};

	for (i = 0; i < len; i++) {
		printf("%s = %d\n", type_name[i], *((unsigned short *)r_buf + i));
	}

	return 0;
}

/*-----CMD ID:78-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_chain
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*	bit 0: Primary channel
*	bit 1: Second channel
*
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_chain(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	*((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}
	if((*s_buf >3) || (0 == *s_buf))
	{
		ENG_LOG("index value:%d, not equal to 1,2,3", *s_buf);
		return -1;
	}

	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:79-----------*/
/********************************************************************
*   name   wlnpi_show_get_chain
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_chain(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char chain = 255;

	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len)
	{
		printf("get_chain err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	chain = *((unsigned char *)r_buf);
	if((chain > 3) || (0 == chain))
	{
		printf("get_chain err,not equal to 1,2,3\n");
		return -1;
	}

	printf("ret: %d :end\n", chain);

	ENG_LOG("ADL leaving %s(), chain = %d, return 0", __func__, chain);

	return 0;
}

/*-----CMD ID:80-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_sbw
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   para: 
*   0 stand for 20M
*   1 stand for 40M
*   2 stand for 80M
*   3 stand for 160M
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_sbw(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	*((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}
	if(*s_buf > 3)
	{
		ENG_LOG("index value:%d, exceed define!", *s_buf);
		return -1;
	}

	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}
/*-----CMD ID:81-----------*/
/********************************************************************
*   name   wlnpi_show_get_sbw
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_sbw(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char sbw = 255;

	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len)
	{
		printf("get_sbw err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	sbw = *( (unsigned char *)r_buf );
	if(sbw > 3)
	{
		printf("get_sbw err\n");
		return -1;
	}

	printf("ret: %d :end\n", sbw);

	ENG_LOG("ADL leaving %s(), chain = %d, return 0", __func__, sbw);

	return 0;
}

/*-----CMD ID:84-----------*/
int wlnpi_cmd_set_ampdu_cnt(int argc, char **argv, signed char *s_buf, int *s_len )
{
	char *err;

	if(3 != argc) {
	    printf("%s invalid argc : %d\n", __func__, argc);
	    return -1;
	}
	printf("direction: %s\n", argv[0]);
	printf("ampdu_cnt: %s\n", argv[1]);
	printf("amsdu_cnt: %s\n", argv[2]);
	*((signed int *)s_buf) = strtol(argv[0], &err, 10);
	*((signed int *)(s_buf +1)) = strtol(argv[1], &err, 10);
	*((signed int *)(s_buf +2)) = strtol(argv[2], &err, 10);
	if (*s_buf < 0 || * s_buf > 2
		|| *(s_buf +1) < 0 || *(s_buf +1)  > 64
		|| *(s_buf +2) < 0 || *(s_buf +2)  > 3) {
		ENG_LOG("%s, invalid argv!\n",__func__);
		return -1;
	}
	*s_len = 3;
	return 0;
}

/*-----CMD ID:88-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_fec
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   para: 0 stand for BCC 1 stand for LDPC
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_fec(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	*((unsigned int *)s_buf) = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}
	if((0 != *s_buf) &&(1 != *s_buf))
	{
		ENG_LOG("index value:%d, not equal to 0 or 1", *s_buf);
		return -1;
	}

	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:89-----------*/
/********************************************************************
*   name   wlnpi_show_get_fec
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_fec(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char fec = 255;

	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len)
	{
		printf("get_fec err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	fec = *( (unsigned char *)r_buf );
	if((0 != fec) &&(1 != fec))
	{
		printf("get_fec err\n");
		return -1;
	}

	printf("ret: %d :end\n", fec);

	ENG_LOG("ADL leaving %s(), fec = %d, return 0", __func__, fec);

	return 0;
}



/*-----CMD ID:94-----------*/
int wlnpi_cmd_set_dpd_enable(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val != 0 && val != 1)
	{
		ENG_LOG("param error! val = %ld", val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

int wlnpi_cmd_set_cbank_reg(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	char *err;
	long val;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val > 255)
	{
		ENG_LOG("%s param = %ld", __func__, val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:115-----------*/
/********************************************************************
*   name   wlnpi_cmd_set_prot_mode
*   ---------------------------
*   description: set protection mode
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_set_prot_mode(int argc, char **argv,  unsigned char *s_buf, int *s_len)
{
	char **err = NULL;
	unsigned char *mode = s_buf;
	unsigned char *prot_mode = s_buf + 1;
	int val;

	ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

	if(2 != argc)
	{
		ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
		return -1;
	}

	*mode = strtoul(argv[0], err, 10);
	if(err)
	{
	ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
	return -1;
	}

	val = strtoul(argv[1], err, 10);
	if (err || val > 0xff)
	{
		ENG_LOG("prot_mode = %d err", val);
	return -1;
	}

	*prot_mode = val;

	*s_len = 2;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:116-----------*/
/********************************************************************
*   name   wlnpi_cmd_get_prot_mode
*   ---------------------------
*   description: get protection mode
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_cmd_get_prot_mode(int argc, char **argv,  unsigned char *s_buf, int *s_len)
{
	char **err = NULL;
	unsigned char *mode = s_buf;
	int val;

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

	if(1 != argc)
	{
		ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
		return -1;
	}

	val = strtoul(argv[0], err, 10);
	if(err && val > 0xff)
	{
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
	}

	*mode = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:116-----------*/
/********************************************************************
*   name   wlnpi_show_get_prot_mode
*   ---------------------------
*   description: show protection mode
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int wlnpi_show_get_prot_mode(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned int retcnt = 0;
	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);


	if (r_len == 1)
	{
		retcnt = *((unsigned char*)(r_buf+0));
	}
	else if (r_len == 4)
	{
		retcnt = *((unsigned int *)(r_buf+0));
	}
	else
	{
		printf("get_ar_retcnt err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	printf("ret: %d :end\n", retcnt);

	ENG_LOG("ADL leaving %s(), retcnt = %d, return 0", __func__, retcnt);

	return 0;
}

/*-----CMD ID:117-----------*/
/********************************************************************
 *   name   wlnpi_cmd_set_threshold
 *   ---------------------------
 *   description: set threshold rts frag
 *   ----------------------------
 *   para        IN/OUT      type            note
 *   ----------------------------------------------------
 *   return
 *   0:exec successful
 *   -1:error
 *   ------------------
 *   other:
 *
 ********************************************************************/
int wlnpi_cmd_set_threshold(int argc, char **argv,  unsigned char *s_buf, int *s_len)
{
    char **err = NULL;
	unsigned char *mode = s_buf;
    unsigned int *rts = (unsigned int *)(s_buf + 1);
    unsigned int *frag = (unsigned int *)(s_buf + 5);

    ENG_LOG("ADL entry %s(), argc = %d", __func__, argc);

    if(3 != argc)
    {
		ENG_LOG("ADL leaving %s(), argc error. return -1", __func__);
		return -1;
    }

    *mode = strtoul(argv[0], err, 10);
    if(err)
    {
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
    }

    *rts = strtoul(argv[1], err, 10);
    if(err)
    {
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
    }

    *frag = strtoul(argv[2], err, 10);
    if(err)
    {
		ENG_LOG("ADL leaving %s(), strtol error. return -1", __func__);
		return -1;
    }
    //iwnpi_hex_dump("D:", 2, s_buf, 9);
    *s_len = 9;

    ENG_LOG("ADL leaving %s()", __func__);
    return 0;
}


/*-----CMD ID:118-----------*/
int wlnpi_cmd_set_ar_dbg(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	int i, ret;
	int *param;
	unsigned long int val;

	if ((argc != 4) || (s_buf == NULL))
	{
		ENG_LOG("ADL leaving %s(), argc = %d ERR\n", __func__, argc);
		return -1;
	}

	ret = str2digit(argv[0], &val);
	if (ret < 0)
	{
		ENG_LOG("ADL leaving %s(), argv[0]=%s ERR.", __func__, argv[0]);
		return -1;
	}

	*s_buf = val & 0xff;

	param = (int *)(s_buf + 1);

	for (i = 0; i < 3; i++)
	{
		ret = str2digit(argv[i + 1], &val);
		if (ret < 0)
		{
			ENG_LOG("ADL leaving %s(), argv[%d]=%s ERR.", __func__, i + 1, argv[i+1]);
			return -1;
		}

		param[i] = (int)(val & 0xffffffff);
	}

	*s_len = sizeof(*s_buf) + 3 * sizeof(*param);
	ENG_LOG("ADL leaving %s(), dbg_id = %d, param1 = %d, param2 = %d, param3 = %d",
			             __func__, *s_buf, param[0], param[1], param[2]);

	return 0;
}

/*-----CMD ID:119-----------*/
int wlnpi_cmd_set_pm_ctl(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
    char *err;
    if(1 != argc)
        return -1;
    *(unsigned int *)s_buf = strtol(argv[0], &err, 10);
    *s_len = 4;
    if(err == argv[0])
        return -1;
    return 0;
}

/*-----CMD ID:120-----------*/
int wlnpi_show_get_pm_ctl(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
    unsigned int ctl = 0;

    ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

    if(4 != r_len)
    {
        printf("get_pm ctl err\n");
        ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

        return -1;
    }

    ctl = *(unsigned int *)r_buf;
    printf("ret: %d:end\n", ctl);

    ENG_LOG("ADL leaving %s(), pm_ctl = %d, return 0", __func__, ctl);

    return 0;
}

int wlnpi_show_get_pa_infor(struct wlnpi_cmd_t *cmd,
			    unsigned char *r_buf,
			    int r_len)
{
#define PAINFO_TYPE_STR_LEN	20
#define PAINFO_TYPE_CMN		1
#define PAINFO_TYPE_TX		2
#define PAINFO_TYPE_RX		3

	int i = 0,
	    offset = 0,
	    item_num = 0,
	    sz_wlnpi_pa_info_header_t = sizeof(struct wlnpi_pa_info_header_t);
	char pa_info_type_str[PAINFO_TYPE_STR_LEN] = {0};
	unsigned int *pa_info_body = NULL;
	struct wlnpi_pa_info_header_t *pa_info_header = NULL;
	struct wlnpi_pa_info_reg_t *pa_info_reg = NULL;
	struct wlnpi_pa_info_reg_t pa_info_reg_cmn[] =
	{
		{0x400f1000, "REG_WIFI_MAC_RTN_MAC_CONTROL"},
		{0x400fc000, "REG_WIFI_MAC_MH_PD_MH_CTL"},
		{0x400fc050, "REG_WIFI_MAC_MH_PD_STA_LUT_CTRL"},
		{0x400fc054, "REG_WIFI_MAC_MH_PD_STA_LUT_VLD"},
		{0x400fc060, "REG_WIFI_MAC_MH_PD_AMSDU_LUT_CTRL"},
		{0x400fc064, "REG_WIFI_MAC_MH_PD_AMSDU_LUT_VLD"}
	};
	struct wlnpi_pa_info_reg_t pa_info_reg_tx[] =
	{
		{0x400f2000, "REG_WIFI_MAC_RTN_PA_TX_CON1"},
		{0x400f2004, "REG_WIFI_MAC_RTN_PA_TX_CON2"},
		{0x400f201c, "REG_WIFI_MAC_RTN_AC_TX_SUSPEND_STATUS"},
		{0x400f2058, "REG_WIFI_MAC_RTN_TXOP_LIMIT_BE_BK"},
		{0x400f205c, "REG_WIFI_MAC_RTN_TXOP_LIMIT_VO_VI"},
		{0x400f2090, "REG_WIFI_MAC_RTN_MSDU_LIFETIME_LIMIT"},
		{0x400f2094, "REG_WIFI_MAC_RTN_BE_BK_LIFETIME_LIMIT"},
		{0x400f2098, "REG_WIFI_MAC_RTN_VO_VI_LIFETIME_LIMIT"},
		{0x400f20c8, "REG_WIFI_MAC_RTN_STA_TXQ_PS_STATUS"},
		{0x400f20cc, "REG_WIFI_MAC_RTN_MCC_CTL"},
		{0x400f20d0, "REG_WIFI_MAC_RTN_MCC_STA_TERMINATE_MASK"},
		{0x400f20f4, "REG_WIFI_MAC_RTN_PPDU_MAX_TIME"},
		{0x400f20f8, "REG_WIFI_MAC_RTN_PPDU_MAX_TIME2"},
		{0x400f2134, "REG_WIFI_MAC_RTN_P2P_TERMINATE_BITMAP"},
		{0x400f2138, "REG_WIFI_MAC_RTN_PS_TERMINATE_BYPASS_BITMAP"},
		{0x400f8000, "REG_WIFI_PA_PD_PA_TX_STATUS"},
		{0x400f8004, "REG_WIFI_PA_PD_TX_INT_STATUS"},
		{0x400f8008, "REG_WIFI_PA_PD_TX_INT_STATUS_CLR"},
		{0x400f800c, "REG_WIFI_PA_PD_TX_INT_MASK"},
		{0x400f8010, "REG_WIFI_PA_PD_TX_INT_AFTER_MASK"},
		{0x400f8014, "REG_WIFI_PA_PD_TX_INT_POINTER"},
		{0x400f8018, "REG_WIFI_PA_PD_TX_INT_NUM"},
		{0x400f801c, "REG_WIFI_PA_PD_TX_HIQ_INT_POINTER"},
		{0x400f8020, "REG_WIFI_PA_PD_TX_ERROR_CODE"},
		{0x400fc004, "REG_WIFI_MAC_MH_PD_MH_TX_SUSPEND_CTL"},
		{0x400fc008, "REG_WIFI_MAC_MH_PD_MH_TX_STS"}
	};
	struct wlnpi_pa_info_reg_t pa_info_reg_rx[] =
	{
		{0x400f403c, "REG_WIFI_MAC_RTN_RX_MAX_MPDU_LENGTH"},
		{0x400f4040, "REG_WIFI_MAC_RTN_RX_BUFFER_SIZE"},
		{0x400f4400, "REG_WIFI_MAC_RTN_RX_DMM_THRESHOLD"},
		{0x400f4500, "REG_WIFI_MAC_RTN_RX_PHY_RX_END_CNT"},
		{0x400f4504, "REG_WIFI_MAC_RTN_RX_MPDU_ORI_FCS_CNT"},
		{0x400f4508, "REG_WIFI_MAC_RTN_RX_MPDU_FCS_CNT"},
		{0x400f450c, "REG_WIFI_MAC_RTN_RX_FCSFAIL_CNT"},
		{0x400f4510, "REG_WIFI_MAC_RTN_RX_ABORT_CNT"},
		{0x400f4514, "REG_WIFI_MAC_RTN_RX_ABORT_CNT2"},
		{0x400f4518, "REG_WIFI_MAC_RTN_RX_HANG_ABORT_CNT"},
		{0x400f451c, "REG_WIFI_MAC_RTN_RX_EMPTY_CNT"},
		{0x400f4520, "REG_WIFI_MAC_RTN_RX_RESPONSE_CNT"},
		{0x400fa004, "REG_WIFI_PA_PD_PA_RX_STATUS1"},
		{0x400fc024, "REG_WIFI_MAC_MH_PD_MH_RX_MAX_FRAME_LEN"}
	};

	ENG_LOG("ADL entry %s(),id=%d, %s, r_len = %d",
			__func__, cmd->id, cmd->name, r_len);

	if ((r_buf == NULL) || (1 > r_len) || (512 < r_len))
	{
		printf("get_cp2_infor cmd_name:%s, buf=0x%p, len=%d err\n",
				cmd->name, r_buf, r_len);
		ENG_LOG("ADL leaving %s(), r_buf=0x%p, r_len=%d ERROR, return -1",
				__func__, r_buf, r_len);
		return -1;
	}

	pa_info_header = (struct wlnpi_pa_info_header_t *)r_buf;
	while ((offset + sz_wlnpi_pa_info_header_t + pa_info_header->length) <= r_len)
	{
		pa_info_body = (unsigned int *)((char *)(pa_info_header) + sz_wlnpi_pa_info_header_t);
		item_num = 0;
		switch(pa_info_header->type)
		{
		case PAINFO_TYPE_CMN:
			item_num = sizeof(pa_info_reg_cmn)/sizeof(struct wlnpi_pa_info_reg_t);
			if((item_num * 4) != pa_info_header->length)
			{
				pa_info_reg = NULL;
			}
			else
			{
				snprintf(pa_info_type_str, PAINFO_TYPE_STR_LEN, "Common");
				pa_info_reg = pa_info_reg_cmn;
			}
			break;
		case PAINFO_TYPE_TX:
			item_num = sizeof(pa_info_reg_tx)/sizeof(struct wlnpi_pa_info_reg_t);
			if((item_num * 4) != pa_info_header->length)
			{
				pa_info_reg = NULL;
			}
			else
			{
				snprintf(pa_info_type_str, PAINFO_TYPE_STR_LEN, "Tx");
				pa_info_reg = pa_info_reg_tx;
			}
			break;
		case PAINFO_TYPE_RX:
			item_num = sizeof(pa_info_reg_rx)/sizeof(struct wlnpi_pa_info_reg_t);
			if((item_num * 4) != pa_info_header->length)
			{
				pa_info_reg = NULL;
			}
			else
			{
				snprintf(pa_info_type_str, PAINFO_TYPE_STR_LEN, "Rx");
				pa_info_reg = pa_info_reg_rx;
			}
			break;
		default:
			pa_info_reg = NULL;
		}

		if(pa_info_reg != NULL)
		{
			printf("\r\n============= TYPE:%d(%s) =============\r\n",
					pa_info_header->type, pa_info_type_str);
			for (i = 0; i < item_num; i++)
			{
				printf("[0x%08x] %-50s:0x%08x\r\n",
						pa_info_reg[i].reg, pa_info_reg[i].name, pa_info_body[i]);
			}
		}
		else
		{
			printf("\r\n============= format mismatch, output raw data =============\r\n");
			printf("TYPE:%d, Length:%d\r\n",
					pa_info_header->type, pa_info_header->length);
			for(i = 0; i < (pa_info_header->length/4); i++)
			{
				printf("[%02d] 0x%08x\r\n", i, pa_info_body[i]);
			}
		}

		offset += (sz_wlnpi_pa_info_header_t + pa_info_header->length);
		pa_info_header = (struct wlnpi_pa_info_header_t *)(r_buf + offset);
	}

	if (offset != r_len)
	{
		printf("length mismatch: offset=%d, r_len=%d\n", offset, r_len);
		ENG_LOG("ADL leaving %s(), r_buf=0x%p, r_len=%d offset=%d ERROR, return -1",
				__func__, r_buf, r_len, offset);
		return -1;
	}

	ENG_LOG("ADL leaving %s(), return 0", __func__);
	return 0;

#undef PAINFO_TYPE_STR_LEN
#undef PAINFO_TYPE_CMN
#undef PAINFO_TYPE_TX
#undef PAINFO_TYPE_RX
}

int wlnpi_show_tx_status(struct wlnpi_cmd_t *cmd,
			     unsigned char *r_buf,
			     int r_len)
{
	int i = 0;
	struct tx_status_struct *tx_status = (struct tx_status_struct *)r_buf;

	ENG_LOG("ADL entry %s(),id=%d, %s, r_len = %d",
		    __func__, cmd->id, cmd->name, r_len);

	if ((r_buf == NULL) || (1 > r_len) || (512 < r_len)) {
		printf("get_cp2_infor cmd_name:%s err\n", cmd->name);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	if ((int)sizeof(struct tx_status_struct) != r_len)
	{
		printf("length mismatch: need=%lu, r_len=%d\n",
				sizeof(struct tx_status_struct), r_len);
		ENG_LOG("ADL leaving %s(), r_buf=0x%p, r_len=%d need=%lu ERROR, return -1",
				__func__, r_buf, r_len, sizeof(struct tx_status_struct));
		return -1;
	}

	printf("============= tx status information =============\r\n");

	for (i = 0; i < 4; i++) {
		printf("tx_pkt_cnt[%d]:%15u\r\n", i,
		       tx_status->tx_pkt_cnt[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("tx_suc_cnt[%d]:%15u\r\n", i,
		       tx_status->tx_suc_cnt[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("tx_fail_cnt[%d]:%14u\r\n", i,
		       tx_status->tx_fail_cnt[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("tx_retry[%d]:%17u\r\n", i,
		       tx_status->tx_retry[i]);
	}
	printf("tx_err_cnt:%18u\r\n", tx_status->tx_err_cnt);
	for (i = 0; i < 4; i++) {
		printf("tx_fail_reason_cnt[%d]:%7u\r\n", i,
		       tx_status->tx_fail_reason_cnt[i]);
	}
	printf("rts_succ_cnt:%16u\r\n", tx_status->rts_success_cnt);
	printf("cts_fail_cnt:%16u\r\n", tx_status->cts_fail_cnt);
	printf("ampdu_retry_cnt:%13u\r\n", tx_status->ampdu_retry_cnt);
	printf("ba_rx_fail_cnt:%14u\r\n", tx_status->ba_rx_fail_cnt);
	for (i = 0; i < 4; i++) {
		printf("color_num_sdio[%d]:%11u\r\n", i,
		       tx_status->color_num_sdio[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("color_num_mac[%d]:%12u\r\n", i,
		       tx_status->color_num_mac[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("color_num_txc[%d]:%12u\r\n", i,
		       tx_status->color_num_txc[i]);
	}

	printf("=================================================\r\n");

	ENG_LOG("ADL leaving %s(), return 0", __func__);
	return 0;
}

int wlnpi_show_rx_status(struct wlnpi_cmd_t *cmd,
			     unsigned char *r_buf,
			     int r_len)
{
	int i = 0;
	struct rx_status_struct *rx_status = (struct rx_status_struct *)r_buf;

	ENG_LOG("ADL entry %s(),id=%d, %s, r_len = %d",
			__func__,cmd->id, cmd->name, r_len);

	if ((r_buf == NULL) || (1 > r_len) || (512 < r_len)) {
		printf("get_cp2_infor cmd_name:%s err\n", cmd->name);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	if ((int)sizeof(struct rx_status_struct) != r_len)
	{
		printf("length mismatch: need=%lu, r_len=%d\n",
				sizeof(struct rx_status_struct), r_len);
		ENG_LOG("ADL leaving %s(), r_buf=0x%p, r_len=%d need=%lu ERROR, return -1",
				__func__, r_buf, r_len, sizeof(struct rx_status_struct));
		return -1;
	}

	printf("============= rx status information =============\r\n");

	for (i = 0; i < 4; i++) {
		printf("rx_pkt_cnt[%d]:%25u\r\n", i,
		       rx_status->rx_pkt_cnt[i]);
	}
	for (i = 0; i < 4; i++) {
		printf("rx_retry_pkt_cnt[%d]:%19u\r\n", i,
		       rx_status->rx_retry_pkt_cnt[i]);
	}
	printf("rx_su_beamformed_pkt_cnt:%14u\r\n",
	       rx_status->rx_su_beamformed_pkt_cnt);
	printf("rx_mu_pkt_cnt:%25u\r\n",
	       rx_status->rx_mu_pkt_cnt);
	for (i = 0; i < 16; i++) {
		printf("rx_11n_mcs_cnt[%02d]:%20u\r\n", i,
		       rx_status->rx_11n_mcs_cnt[i]);
	}
	for (i = 0; i < 16; i++) {
		printf("rx_11n_sgi_cnt[%02d]:%20u\r\n", i,
		       rx_status->rx_11n_sgi_cnt[i]);
	}
	for (i = 0; i < 10; i++) {
		printf("rx_11ac_mcs_cnt[%d]:%20u\r\n", i,
		       rx_status->rx_11ac_mcs_cnt[i]);
	}
	for (i = 0; i < 10; i++) {
		printf("rx_11ac_sgi_cnt[%d]:%20u\r\n", i,
		       rx_status->rx_11ac_sgi_cnt[i]);
	}
	for (i = 0; i < 2; i++) {
		printf("rx_11ac_stream_cnt[%d]:%17u\r\n", i,
		       rx_status->rx_11ac_stream_cnt[i]);
	}
	for (i = 0; i < 3; i++) {
		printf("rx_bandwidth_cnt[%d]:%19u\r\n", i,
		       rx_status->rx_bandwidth_cnt[i]);
	}
	for (i = 0; i < 3; i++) {
		printf("last_rxdata_rssi1[%d]:%18u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_rssi1);
		printf("last_rxdata_rssi2[%d]:%18u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_rssi2);
		printf("last_rxdata_snr1[%d]:%19u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_snr1);
		printf("last_rxdata_snr2[%d]:%19u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_snr2);
		printf("last_rxdata_snr_combo[%d]:%14u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_snr_combo);
		printf("last_rxdata_snr_l[%d]:%18u\r\n", i,
		       rx_status->rx_rssi[i].last_rxdata_snr_l);
	}
	for (i = 0; i < 6; i++) {
		printf("rxc_isr_cnt[%d]:%24u\r\n", i,
		       rx_status->rxc_isr_cnt[i]);
	}
	for (i = 0; i < 5; i++) {
		printf("rxq_buffer_rqst_isr_cnt[%d]:%12u\r\n", i,
		       rx_status->rxq_buffer_rqst_isr_cnt[i]);
	}
	for (i = 0; i < 5; i++) {
		printf("req_tgrt_bu_num[%d]:%20u\r\n", i,
		       rx_status->req_tgrt_bu_num[i]);
	}
	for (i = 0; i < 3; i++) {
		printf("rx_alloc_pkt_num[%d]:%19u\r\n", i,
		       rx_status->rx_alloc_pkt_num[i]);
	}

	printf("=================================================\r\n");

	ENG_LOG("ADL leaving %s(), return 0", __func__);
	return 0;
}

int wlnpi_show_get_sta_lut_status(struct wlnpi_cmd_t *cmd,
				  unsigned char *r_buf,
				  int r_len)
{
/* instance mode definition */
#define INSTANCE_MODE_STR_LEN	20
#define INSTANCE_MODE_STA		0
#define INSTANCE_MODE_AP		1
#define INSTANCE_MODE_P2P_GC	2
#define INSTANCE_MODE_P2P_GO	3
#define INSTANCE_MODE_NPI		4
#define INSTANCE_MODE_IBSS		5
#define INSTANCE_MODE_NAN		6
#define INSTANCE_MODE_P2P_DEV	7
#define INSTANCE_MODE_INVALID	0xFF

/* STALUT state bit definition */
#define STALUT_STATE_STA_ASSOC_JOIN_DONE_BIT	BIT(0)
#define STALUT_STATE_STA_RSNA_HS_DONE_BIT		BIT(1)
#define STALUT_STATE_STA_CNTR_MSR_BIT			BIT(2)
#define STALUT_STATE_STA_GET_IP_DONE_BIT		BIT(3)

/* STALUT capability bit definition */
#define STALUT_CAP_QOS_BIT				BIT(0)
#define STALUT_CAP_HT_BIT				BIT(1)
#define STALUT_CAP_HTC_BIT				BIT(2)
#define STALUT_CAP_VHT_BIT				BIT(3)
#define STALUT_CAP_TXOP_BIT				BIT(4)
#define STALUT_CAP_SMPS_BITS			(BIT(5)|BIT(6))
#define STALUT_CAP_TDLS_BIT				BIT(7)
#define STALUT_CAP_WEP_BIT				BIT(8)
#define STALUT_CAP_RSNA_BIT				BIT(9)
#define STALUT_CAP_KEY_TYPE_BIT			BIT(10)
#define STALUT_CAP_WAPI_BIT				BIT(11)
#define STALUT_CAP_WDS_BIT				BIT(12)
#define STALUT_CAP_VHT_MCS9_BW20_BIT	BIT(13)

	int i = 0,
	    offset = 0,
	    sz_wlnpi_sta_lut_t = sizeof(struct wlnpi_sta_lut_t);
	char instance_mode_str[INSTANCE_MODE_STR_LEN] = {0};
	unsigned int instance_mode = 0,
				 *slut_raw = NULL;
	struct wlnpi_sta_lut_t *slut = NULL;

	ENG_LOG("ADL entry %s(),id=%d, %s, r_len = %d",
			__func__, cmd->id, cmd->name, r_len);

	if ((r_buf == NULL) || (1 > r_len) || (512 < r_len)) {
		printf("get_cp2_infor cmd_name:%s err\n", cmd->name);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	instance_mode = *((unsigned int *)r_buf);
	offset += 4;
	switch(instance_mode)
	{
	case INSTANCE_MODE_STA:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "STA");
		break;
	case INSTANCE_MODE_AP:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "AP");
		break;
	case INSTANCE_MODE_P2P_GC:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "P2P_GC");
		break;
	case INSTANCE_MODE_P2P_GO:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "P2P_GO");
		break;
	case INSTANCE_MODE_NPI:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "NPI");
		break;
	case INSTANCE_MODE_IBSS:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "IBSS");
		break;
	case INSTANCE_MODE_NAN:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "NAN");
		break;
	case INSTANCE_MODE_P2P_DEV:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "P2P_DEV");
		break;
	case INSTANCE_MODE_INVALID:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "INVALID");
		break;
	default:
		snprintf(instance_mode_str, INSTANCE_MODE_STR_LEN, "UNKNOWN");
	}

	printf("============= Mode:%u(%s), Num Of STA:%d =============\r\n",
			instance_mode, instance_mode_str,
			(r_len-offset)/sz_wlnpi_sta_lut_t);

	while ((offset + sz_wlnpi_sta_lut_t) <= r_len)
	{
		slut = (struct wlnpi_sta_lut_t *)(r_buf + offset);
		slut_raw = (unsigned int *)slut;

		printf("[STALUT#%02d, Index:%02u]\r\n", i++, slut->lut_idx);

		printf("WORD0-WORD1:0x%08x-0x%08x\r\n", slut_raw[1], slut_raw[2]);
		printf("      mac_addr:%02x-%02x-%02x-%02x-%02x-%02x (hex)\r\n", slut->mac_addr_h[1],
				slut->mac_addr_h[0], slut->mac_addr_l[3], slut->mac_addr_l[2],
				slut->mac_addr_l[1], slut->mac_addr_l[0]);
		printf("      mac_index:%u\r\n", slut->mac_index);
		printf("      state:0x%x\r\n", slut->state);
		printf("        assoc/join:%d, rsna handshake:%d\r\n",
				(slut->state & STALUT_STATE_STA_ASSOC_JOIN_DONE_BIT)? 1:0,
				(slut->state & STALUT_STATE_STA_RSNA_HS_DONE_BIT)? 1:0);
		printf("        countermeasure in progress:%d, get ip:%d\r\n",
				(slut->state & STALUT_STATE_STA_CNTR_MSR_BIT)? 1:0,
				(slut->state & STALUT_STATE_STA_GET_IP_DONE_BIT)? 1:0);
		printf("      cipher_type_0:%u\r\n", slut->cipher_type_0);
		printf("      cipher_type_1:%u\r\n", slut->cipher_type_1);

		printf("WORD2:0x%08x\r\n", slut_raw[3]);
		printf("      capability:0x%x\r\n", slut->capability);
		printf("        qos:%d, ht:%d, htc:%d, vht:%d, txop_ps:%d\r\n",
				(slut->capability & STALUT_CAP_QOS_BIT)? 1:0,
				(slut->capability & STALUT_CAP_HT_BIT)? 1:0,
				(slut->capability & STALUT_CAP_HTC_BIT)? 1:0,
				(slut->capability & STALUT_CAP_VHT_BIT)? 1:0,
				(slut->capability & STALUT_CAP_TXOP_BIT)? 1:0);
		printf("        smps:0x%x, tdls:%d, wep:%d, rsna:%d, key type:%d\r\n",
				(slut->capability & STALUT_CAP_SMPS_BITS) >> 5,
				(slut->capability & STALUT_CAP_TDLS_BIT)? 1:0,
				(slut->capability & STALUT_CAP_WEP_BIT)? 1:0,
				(slut->capability & STALUT_CAP_RSNA_BIT)? 1:0,
				(slut->capability & STALUT_CAP_KEY_TYPE_BIT)? 1:0);
		printf("        wapi:%d, wds:%d, vht_mcs9_bw20:%d\r\n",
				(slut->capability & STALUT_CAP_WAPI_BIT)? 1:0,
				(slut->capability & STALUT_CAP_WDS_BIT)? 1:0,
				(slut->capability & STALUT_CAP_VHT_MCS9_BW20_BIT)? 1:0);
		printf("      ba_for_tx_path:0x%x\r\n", slut->ba_for_tx_path);
		printf("      tdls_cnt_idx:%u\r\n", slut->tdls_cnt_idx);
		printf("      tdls_cnt_en:%u\r\n", slut->tdls_cnt_en);
		printf("      ba_for_rx_path:0x%x\r\n", slut->ba_for_rx_path);

		printf("WORD3:0x%08x\r\n", slut_raw[4]);
		printf("      tx_amsdu_lut_index_bk:%u\r\n", slut->tx_amsdu_lut_index_bk);
		printf("      tx_amsdu_lut_index_be:%u\r\n", slut->tx_amsdu_lut_index_be);
		printf("      tx_amsdu_lut_index_vi:%u\r\n", slut->tx_amsdu_lut_index_vi);
		printf("      tx_amsdu_lut_index_vo:%u\r\n", slut->tx_amsdu_lut_index_vo);

		printf("WORD4-WORD7:0x%08x-0x%08x-0x%08x-0x%08x\r\n",
				slut_raw[5], slut_raw[6], slut_raw[7], slut_raw[8]);
		printf("      txq_rate_bk_be:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x (hex)\r\n",
				slut->txq_rate_bk_be[0], slut->txq_rate_bk_be[1], slut->txq_rate_bk_be[2],
				slut->txq_rate_bk_be[3], slut->txq_rate_bk_be[4], slut->txq_rate_bk_be[5],
				slut->txq_rate_bk_be[6], slut->txq_rate_bk_be[7]);
		printf("      txq_power_bk_be:%02u-%02u-%02u-%02u-%02u-%02u-%02u-%02u\r\n",
				slut->txq_power_bk_be[0], slut->txq_power_bk_be[1], slut->txq_power_bk_be[2],
				slut->txq_power_bk_be[3], slut->txq_power_bk_be[4], slut->txq_power_bk_be[5],
				slut->txq_power_bk_be[6], slut->txq_power_bk_be[7]);

		printf("WORD8-WORD11:0x%08x-0x%08x-0x%08x-0x%08x\r\n",
				slut_raw[9], slut_raw[10], slut_raw[11], slut_raw[12]);
		printf("      txq_rate_vo_vi:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x (hex)\r\n",
				slut->txq_rate_vo_vi[0], slut->txq_rate_vo_vi[1], slut->txq_rate_vo_vi[2],
				slut->txq_rate_vo_vi[3], slut->txq_rate_vo_vi[4], slut->txq_rate_vo_vi[5],
				slut->txq_rate_vo_vi[6], slut->txq_rate_vo_vi[7]);
		printf("      txq_power_vo_vi:%02u-%02u-%02u-%02u-%02u-%02u-%02u-%02u\r\n",
				slut->txq_power_vo_vi[0], slut->txq_power_vo_vi[1], slut->txq_power_vo_vi[2],
				slut->txq_power_vo_vi[3], slut->txq_power_vo_vi[4], slut->txq_power_vo_vi[5],
				slut->txq_power_vo_vi[6], slut->txq_power_vo_vi[7]);

		printf("WORD12-WORD15:0x%08x-0x%08x-0x%08x-0x%08x\r\n",
				slut_raw[13], slut_raw[14], slut_raw[15], slut_raw[16]);
		printf("      swq_rate:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x (hex)\r\n",
				slut->swq_rate[0], slut->swq_rate[1], slut->swq_rate[2],
				slut->swq_rate[3], slut->swq_rate[4], slut->swq_rate[5],
				slut->swq_rate[6], slut->swq_rate[7]);
		printf("      swq_power:%02u-%02u-%02u-%02u-%02u-%02u-%02u-%02u\r\n",
				slut->swq_power[0], slut->swq_power[1], slut->swq_power[2],
				slut->swq_power[3], slut->swq_power[4], slut->swq_power[5],
				slut->swq_power[6], slut->swq_power[7]);

		printf("WORD16:0x%08x\r\n", slut_raw[17]);
		printf("      gi_for_bk_be:0x%x\r\n", slut->gi_for_bk_be);
		printf("      gi_for_vo_vi:0x%x\r\n", slut->gi_for_vo_vi);
		printf("      gi_for_swq:0x%x\r\n", slut->gi_for_swq);
		printf("      gi_for_bw:0x%x\r\n", slut->gi_for_bw);
		printf("      prot_type:%u\r\n", slut->prot_type);

		printf("WORD17:0x%08x\r\n", slut_raw[18]);
		printf("      rate_id_for_txq_ac_bk_be:%u\r\n", slut->rate_id_for_txq_ac_bk_be);
		printf("      rate_id_for_txq_ac_vo_vi:%u\r\n", slut->rate_id_for_txq_ac_vo_vi);
		printf("      rate_id_for_swq:%u\r\n", slut->rate_id_for_swq);
		printf("      green_power_offset:%u\r\n", slut->green_power_offset);

		printf("WORD18:0x%08x\r\n", slut_raw[19]);
		printf("      alarm_threshold:%u\r\n", slut->alarm_threshold);
		printf("      cck_prot_rate:0x%x\r\n", slut->cck_prot_rate);
		printf("      ofdm_prot_rate:0x%x\r\n", slut->ofdm_prot_rate);

		printf("WORD19:0x%08x\r\n", slut_raw[20]);
		printf("      partial_aid:%u\r\n", slut->partial_aid);
		printf("      group_id:%u\r\n", slut->group_id);
		printf("      prot_en:%u\r\n", slut->prot_en);
		printf("      prot_thr:%u\r\n", slut->prot_thr);

		printf("WORD20:0x%08x\r\n", slut_raw[21]);
		printf("      phy_tx_mode:\r\n");
		printf("        e_frm_fmt:%u, u1_ch_width:%u, u1_sig_width:%u, b_smoothing:%u\r\n",
				slut->phy_tx_mode.e_frm_fmt, slut->phy_tx_mode.u1_ch_width,
				slut->phy_tx_mode.u1_sig_width, slut->phy_tx_mode.b_smoothing);
		printf("        b_sounding:%u, ampdu_aggr:%u, b_ldpc_coding:%u, b_short_gi:%u\r\n",
				slut->phy_tx_mode.b_sounding, slut->phy_tx_mode.ampdu_aggr,
				slut->phy_tx_mode.b_ldpc_coding, slut->phy_tx_mode.b_short_gi);
		printf("        u1_stbc:%u, num_streams:%u, u1_ant_confg:%u, b_vht_beam_forming:%u\r\n",
				slut->phy_tx_mode.u1_stbc, slut->phy_tx_mode.num_streams,
				slut->phy_tx_mode.u1_ant_confg, slut->phy_tx_mode.b_vht_beam_forming);
		printf("        txop_sleep:%u, bandwith_oper:%u, b_msr_depart_time:%u, user_num:%u\r\n",
				slut->phy_tx_mode.txop_sleep, slut->phy_tx_mode.bandwith_oper,
				slut->phy_tx_mode.b_msr_depart_time, slut->phy_tx_mode.user_num);

		printf("WORD21:0x%08x\r\n", slut_raw[22]);
		printf("      tx_ampdu_lut_index_ac_bk:%u\r\n", slut->tx_ampdu_lut_index_ac_bk);
		printf("      tx_ampdu_lut_index_ac_be:%u\r\n", slut->tx_ampdu_lut_index_ac_be);
		printf("      tx_ampdu_lut_index_ac_vi:%u\r\n", slut->tx_ampdu_lut_index_ac_vi);
		printf("      tx_ampdu_lut_index_ac_vo:%u\r\n", slut->tx_ampdu_lut_index_ac_vo);

		printf("WORD22:0x%08x\r\n", slut_raw[23]);
		printf("      pending_buf_cnt_ac_bk:%u\r\n", slut->pending_buf_cnt_ac_bk);
		printf("      pending_buf_cnt_ac_be:%u\r\n", slut->pending_buf_cnt_ac_be);

		printf("WORD23:0x%08x\r\n", slut_raw[24]);
		printf("      pending_buf_cnt_ac_vi:%u\r\n", slut->pending_buf_cnt_ac_vi);
		printf("      pending_buf_cnt_ac_vo:%u\r\n", slut->pending_buf_cnt_ac_vo);

		printf("WORD24:0x%08x\r\n", slut_raw[25]);
		printf("      rssi_smooth:%u\r\n\r\n", slut->rssi_smooth);

		offset += sz_wlnpi_sta_lut_t;
	}

	if (offset != r_len)
	{
		printf("length mismatch: offset=%d, r_len=%d\n", offset, r_len);
		ENG_LOG("ADL leaving %s(), r_buf=0x%p, r_len=%d offset=%d ERROR, return -1",
				__func__, r_buf, r_len, offset);
		return -1;
	}

	ENG_LOG("ADL leaving %s(), return 0", __func__);
	return 0;

/* instance mode definition */
#undef INSTANCE_MODE_STR_LEN
#undef INSTANCE_MODE_STA
#undef INSTANCE_MODE_AP
#undef INSTANCE_MODE_P2P_GC
#undef INSTANCE_MODE_P2P_GO
#undef INSTANCE_MODE_NPI
#undef INSTANCE_MODE_IBSS
#undef INSTANCE_MODE_NAN
#undef INSTANCE_MODE_P2P_DEV
#undef INSTANCE_MODE_INVALID

/* STALUT state bit definition */
#undef STALUT_STATE_STA_ASSOC_JOIN_DONE_BIT
#undef STALUT_STATE_STA_RSNA_HS_DONE_BIT
#undef STALUT_STATE_STA_CNTR_MSR_BIT
#undef STALUT_STATE_STA_GET_IP_DONE_BIT

/* STALUT capability bit definition */
#undef STALUT_CAP_QOS_BIT
#undef STALUT_CAP_HT_BIT
#undef STALUT_CAP_HTC_BIT
#undef STALUT_CAP_VHT_BIT
#undef STALUT_CAP_TXOP_BIT
#undef STALUT_CAP_SMPS_BITS
#undef STALUT_CAP_TDLS_BIT
#undef STALUT_CAP_WEP_BIT
#undef STALUT_CAP_RSNA_BIT
#undef STALUT_CAP_KEY_TYPE_BIT
#undef STALUT_CAP_WAPI_BIT
#undef STALUT_CAP_WDS_BIT
#undef STALUT_CAP_VHT_MCS9_BW20_BIT
}

int wlnpi_cmd_set_efuse(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
	unsigned char  *index  = s_buf;
	unsigned int   *value = (unsigned int *)(s_buf + 1);
	char *err;

	if(argc != 2 ) {
		ENG_LOG("paramter is not correct.\n");
		return -1;
     	}

	*index =  strtol(argv[0], &err, 10);
      	if(err == argv[0])
		return -1;

      	if (*index != 11) {
		printf("index must be 11, index : %d\n", *index);
		return -1;
	}

	*value =  strtoul(argv[1], &err, 16);
	if(err == argv[1])
		return -1;

	*s_len = 5;
	return 0;
}

int wlnpi_cmd_get_efuse(int argc, char **argv, signed char *s_buf, int *s_len )
{
	signed char  *index  = s_buf;
	char *err;

	if(argc != 1 ) {
		ENG_LOG("paramter is not correct.\n");
		return -1;
     	}

	*index =  strtol(argv[0], &err, 10);
	if(err == argv[0])
		return -1;

	if(*index < 0 || *index > 15) {
		printf("index must be 0 -15, index : %d \n", *index);
		return -1;
	}
 	*s_len = 1;
	return 0;
}

int wlnpi_show_get_efuse(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	if(4 != r_len)
	{
		printf("get_efuse err, the rlen is not 4\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1\n", __func__);

		return -1;
	}
	printf("ret: efuse:%02x%02x%02x%02x :end\n", r_buf[0],r_buf[1], r_buf[2], r_buf[3]);
	snprintf(iwnpi_ret_buf, WLNPI_RESULT_BUF_LEN, "ret: efuse:%02x%02x%02x%02x :end\n", r_buf[0],r_buf[1], r_buf[2], r_buf[3]);
	printf("iwnpi_ret_buf is %s\n", iwnpi_ret_buf);
	return 0;
}

int wlnpi_cmd_set_mac_efuse(int argc, char **argv,  unsigned char *s_buf, int *s_len )
{
    if(2 != argc) {
        printf("%s invalid argc : %d\n", __func__, argc);
        return -1;
    }
    printf("argv[0] : %s\n", argv[0]);
    printf("argv[1] : %s\n", argv[1]);
    mac_addr_a2n(s_buf, argv[0]);
    mac_addr_a2n(s_buf + 6, argv[1]);
    *s_len = 12;

    return 0;
}

int wlnpi_show_get_mac_efuse(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	int i, ret, p;

	ENG_LOG("ADL entry %s(), r_len = %d\n", __func__,r_len);

	printf("ret: wifi_mac: %02x:%02x:%02x:%02x:%02x:%02x bt_mac: %02x:%02x:%02x:%02x:%02x:%02x :end\n",
	         r_buf[0], r_buf[1], r_buf[2],r_buf[3],r_buf[4], r_buf[5],
	         r_buf[6], r_buf[7], r_buf[8],r_buf[9],r_buf[10], r_buf[11]
	         );
	snprintf(iwnpi_ret_buf, WLNPI_RESULT_BUF_LEN, "ret: MAC:%02x%02x%02x%02x%02x%02x,%02x%02x%02x%02x%02x%02x  :end\n",
	            r_buf[0], r_buf[1], r_buf[2],r_buf[3],r_buf[4], r_buf[5], r_buf[6], r_buf[7], r_buf[8],r_buf[9],r_buf[10], r_buf[11]);
	printf("iwnpi_ret_buf is %s\n", iwnpi_ret_buf);

	return 0;
}

/*-----CMD ID:123-----------*/
int wlnpi_show_get_rf_config(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char config1, config2;

	config1 = *r_buf;
	config2 = *(r_buf + 1);
	ENG_LOG("ADL entry %s(), r_len = %d\n", __FUNCTION__,r_len);

	snprintf(iwnpi_ret_buf, WLNPI_RESULT_BUF_LEN, "ret: ANTINFO=%d,%d  :end\n", r_buf[0], r_buf[1]);

	printf("%s\n", iwnpi_ret_buf);
	ENG_LOG("2.4G rf chain: %d, 5G rf chain : %d\n", config1, config2);
	return 0;
}

/*-----CMD ID:124-----------*/
int wlnpi_cmd_set_tssi(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val > 255)
	{
		ENG_LOG("param error! val = %ld", val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:125-----------*/
int wlnpi_show_get_tssi(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	ENG_LOG("ADL entry %s(), r_len = %d\n", __func__,r_len);

	printf("ret: tssi = %d :end\n", *r_buf);
	ENG_LOG("ADL leaving %s(), return 0", __func__);

	return 0;
}

/*-----CMD ID:126-----------*/
int wlnpi_cmd_set_cca_th(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val > 255)
	{
		ENG_LOG("param error! val = %ld", val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:128-----------*/
int wlnpi_cmd_set_beamforming_status(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val > 255)
	{
		ENG_LOG("param error! val = %ld", val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:129-----------*/
int wlnpi_cmd_set_rxstbc_status(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(1 != argc)
	{
		return -1;
	}

	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val > 255)
	{
		ENG_LOG("param error! val = %ld", val);
		return -1;
	}

	*((unsigned char*)s_buf) = val;
	*s_len = 1;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

int wlnpi_cmd_set_lte_avoid(int argc, char **argv, signed char *s_buf, int *s_len)
{
	char *err;

	if(2 != argc) {
	    printf("%s invalid argc : %d\n", __func__, argc);
	    return -1;
	}
	printf("context-id: %s\n", argv[0]);
	printf("user-channel: %s\n", argv[1]);
	*((signed int *)s_buf) = strtol(argv[0], &err, 10);
	*((signed int *)(s_buf +1)) = strtol(argv[1], &err, 10);
	if (*s_buf < 0 || *(s_buf +1) < 0 || *(s_buf +1)  > 14) {
		ENG_LOG("%s, invalid argv!\n",__func__);
		return -1;
	}
	*s_len = 2;
	return 0;
}

int wlnpi_cmd_set_ini_reg_en(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	char *err;

	if(1 != argc) {
	    printf("%s invalid argc : %d\n", __func__, argc);
	    return -1;
	}
	printf("enable value: %s\n", argv[0]);
	*((unsigned int *)s_buf) = (unsigned int)strtol(argv[0], &err, 10);

	if ((*s_buf != 0) && (*s_buf != 1)) {
		ENG_LOG("%s, invalid argv!\n",__func__);
		return -1;
	}
	*s_len = 1;
	return 0;
}

/*-----CMD ID:131-----------*/
int wlnpi_cmd_get_dpd_coeffi(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
	long val;
	char *err;

	ENG_LOG("ADL entry %s(), argc = %d, argv[0] = %s", __func__, argc, argv[0]);
	if(2 != argc)
	{
		printf("paramter error\n");
		return -1;
	}

	/* chain = 0/1 */
	val = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		return -1;
	}

	if (val != 0 && val != 1)
	{
		ENG_LOG("chain error! val = %ld", val);
		return -1;
	}
	*((unsigned char*)s_buf) = val;

	/* lut_index = 0/1/2 */
	val = strtol(argv[1], &err, 10);
	if(err == argv[1])
	{
		return -1;
	}

	if (val != 0 && val != 1 && val != 2)
	{
		ENG_LOG("lut_index error! val = %ld", val);
		return -1;
	}
	*((unsigned char*)s_buf + 1) = val;

	*s_len = 2;

	ENG_LOG("ADL leaving %s()", __func__);
	return 0;
}

/*-----CMD ID:131-----------*/
int wlnpi_show_dpd_coeffi(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	int i;
	ENG_LOG("ADL entry %s(), r_len = %d\n", __func__,r_len);

	printf("ret: coeffi=");
	for (i = 0; i < r_len; i++)
		printf("0x%02x%c", r_buf[i], i == r_len - 1 ? ' ' : ',');

	printf(":end\n");

	ENG_LOG("ADL leaving %s(), return 0", __func__);

	return 0;
}

/*-----CMD ID:136-----------*/
int wlnpi_show_chip_id(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	printf("ret: %s :end\n", r_buf);

	ENG_LOG("ADL leaving %s(), chipid = %s, return 0", __func__, r_buf);

	return 0;
}

/*-----CMD ID:138-----------*/
int wlnpi_show_get_beamforming_status(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char beamf_status = 255;
	FILE *fp = NULL;
	char ret_buf[WLNPI_RESULT_BUF_LEN+1] = {0x00};

	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len) {
		printf("get_beamforming_status err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	beamf_status = *( (unsigned char *)r_buf );
	printf("ret: status: %d :end\n", beamf_status);
	snprintf(ret_buf, WLNPI_RESULT_BUF_LEN, "ret: status: %d :end\n", beamf_status);
	printf("%s", ret_buf);

	if(NULL != (fp = fopen(IWNPI_EXEC_BEAMF_STATUS_FILE, "w+"))) {
		int write_cnt = 0;

		write_cnt = fputs(ret_buf, fp);
		ENG_LOG("ADL %s(), write_cnt = %d ret_buf %s", __func__, write_cnt, ret_buf);

		fclose(fp);
	}

	ENG_LOG("ADL leaving %s(), beamf_status = %d, return 0", __func__, beamf_status);

	return 0;
}

/*-----CMD ID:139-----------*/
int wlnpi_show_get_rxstbc_status(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	unsigned char rxstbc_status = 255;
	FILE *fp = NULL;
	char ret_buf[WLNPI_RESULT_BUF_LEN+1] = {0x00};

	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len) {
		printf("get_rxstbc_status err\n");
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	rxstbc_status = *( (unsigned char *)r_buf );
	printf("ret: status: %d :end\n", rxstbc_status);
	snprintf(ret_buf, WLNPI_RESULT_BUF_LEN, "ret: status: %d :end\n", rxstbc_status);
	printf("%s", ret_buf);

	if(NULL != (fp = fopen(IWNPI_EXEC_RXSTBC_STATUS_FILE, "w+"))) {
		int write_cnt = 0;

		write_cnt = fputs(ret_buf, fp);
		ENG_LOG("ADL %s(), write_cnt = %d ret_buf %s", __func__, write_cnt, ret_buf);

		fclose(fp);
	}
	ENG_LOG("ADL leaving %s(), rxstbc_status = %d, return 0", __func__, rxstbc_status);

	return 0;
}

/*-----CMD ID:147----------*/
int wlnpi_cmd_set_sar_power(int argc, char **argv, unsigned char *s_buf, int *s_len)
{
    char *err;
    unsigned char *tmp = s_buf;
    long  power, type, mode;

	if(3 != argc)
	{
		printf("need 3 paras, please check!\n");
		return -1;
	}

	type = strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		printf("invalid arguments for type\n");
		return -1;
	}

	printf("type is : %ld\n", type);

	*tmp = (unsigned char)type;
	tmp++;

	power = strtol(argv[1], &err, 10);
	if(err == argv[0])
	{
		printf("invalid arguments for power\n");
		return -1;
	}

	printf("power is : %ld\n", power);

	*tmp = (unsigned char)power;
	tmp++;

	mode = strtol(argv[2], &err, 10);
	if(err == argv[0])
	{
		printf("invalid arguments for mode\n");
		return -1;
	}

	printf("mode is : %ld\n", mode);

	*tmp = (unsigned char)mode;
	tmp++;

	*s_len = 3;

	printf("show para : %u, %u, %u\n", s_buf[0], s_buf[1], s_buf[2]);
	ENG_LOG("show para : %u, %u, %u\n", s_buf[0], s_buf[1], s_buf[2]);

    return 0;
}

/*-----CMD ID:148-----------*/
int wlnpi_show_get_sar_power(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{

	int index = 0;
	char value;
	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(8 != r_len)
	{
		printf("get sar power err, len is invalid : %d\n", r_len);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);

		return -1;
	}

	for(index = 0; index < 8; index++) {
		value = (char)r_buf[index];
		printf("the %d byte is : %d\n", index + 1, value);
	}

    return 0;
}

int wlnpi_cmd_set_softap_wfa_para(int argc, char **argv, unsigned char *s_buf,
				  int *s_len)
{
	char *err;
	if(1 != argc) {
	    printf("%s invalid argc : %d\n", __func__, argc);
	    return -1;
	}
	printf("softap wfa pmf para: %s\n", argv[0]);
	*s_buf = (unsigned char)strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		ENG_LOG("ADL %s(), strtol is ERROR, return -1\n", __func__);
		printf("invalid input argv\n");
		return -1;
	}
	if ((*s_buf <= 0) || (*s_buf > 15)) {
		ENG_LOG("ADL %s, invalid argv!\n",__func__);
		printf("invalid argv!\n");
		return -1;
	}
	*s_len = 1;
	return 0;
}

int wlnpi_cmd_get_softap_wfa_para(struct wlnpi_cmd_t *cmd, unsigned char *r_buf, int r_len)
{
	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len)
	{
		printf("get softap wfa pmf para err, r_len: %d\n", r_len);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	printf("ret: %d :end\n", r_buf[0]);
	ENG_LOG("ADL leaving %s(), softap wfa pmf para = %d, return 0", __func__, r_buf[0]);

	return 0;
}

int wlnpi_cmd_set_sta_wfa_para(int argc, char **argv, unsigned char *s_buf,
				  int *s_len)
{
	char *err;
	if(1 != argc)
	{
		printf("%s invalid argc : %d\n", __func__, argc);
		return -1;
	}
	printf("station wfa pmf para: %s\n", argv[0]);
	*s_buf = (unsigned char)strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		ENG_LOG("ADL %s(), strtol is ERROR, return -1\n", __func__);
		printf("invalid input argv\n");
		return -1;
	}
	*s_len = 1;
	printf("show station wfa para : %u\n", s_buf[0]);
	ENG_LOG("show station wfa para : %u\n", s_buf[0]);
	return 0;
}

int wlnpi_cmd_get_sta_wfa_para(struct wlnpi_cmd_t *cmd, unsigned char *r_buf,
				  int r_len)
{
	ENG_LOG("ADL entry %s(), r_len = %d", __func__, r_len);

	if(1 != r_len)
	{
		printf("get station wfa pmf para err, r_len: %d\n", r_len);
		ENG_LOG("ADL leaving %s(), r_len is ERROR, return -1", __func__);
		return -1;
	}

	printf("ret: %d :end\n", r_buf[0]);
	ENG_LOG("ADL leaving %s(), station wfa pmf para = %d, return 0",
		__func__, r_buf[0]);

	return 0;
}

int wlnpi_cmd_set_rand_mac_flag(int argc, char **argv, unsigned char *s_buf,
				  int *s_len)
{
	char *err;
	if(1 != argc) {
	    printf("%s invalid argc : %d\n", __func__, argc);
	    return -1;
	}
	printf("random mac flag: %s\n", argv[0]);
	*s_buf = (unsigned char)strtol(argv[0], &err, 10);
	if(err == argv[0])
	{
		ENG_LOG("ADL %s(), strtol is ERROR, return -1\n", __func__);
		printf("invalid input argv\n");
		return -1;
	}
	if (*s_buf >=2 ) {
		ENG_LOG("ADL %s, invalid argv!\n",__func__);
		printf("invalid argv!\n");
		return -1;
	}
	*s_len = 1;
	return 0;
}

struct wlnpi_cmd_t g_cmd_table[] =
{
	{
		/*-----CMD ID:0-----------*/
        .id    = WLNPI_CMD_START,
        .name  = "start",
        .help  = "start",
        .parse = wlnpi_cmd_start,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:1-----------*/
        .id    = WLNPI_CMD_STOP,
        .name  = "stop",
        .help  = "stop",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:2-----------*/
        .id    = WLNPI_CMD_SET_MAC,
        .name  = "set_mac",
        .help  = "set_mac <xx:xx:xx:xx:xx:xx>",
        .parse = wlnpi_cmd_set_mac,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:3-----------*/
        .id    = WLNPI_CMD_GET_MAC,
        .name  = "get_mac",
        .help  = "get_mac",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_mac,
    },
    {
        /*-----CMD ID:4-----------*/
        .id    = WLNPI_CMD_SET_MAC_FILTER,
        .name  = "set_macfilter",
        .help  = "set_macfilter <NUM>",
        .parse = wlnpi_cmd_set_macfilter,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:5-----------*/
        .id    = WLNPI_CMD_GET_MAC_FILTER,
        .name  = "get_macfilter",
        .help  = "get_macfilter",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_macfilter,
    },
	{
		/*-----CMD ID:6-----------*/
        .id    = WLNPI_CMD_SET_CHANNEL,
        .name  = "set_channel",
        .help  = "set_channel [primary20_CH][center_CH]",
        .parse = wlnpi_cmd_set_channel,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:7-----------*/
        .id    = WLNPI_CMD_GET_CHANNEL,
        .name  = "get_channel",
        .help  = "get_channel",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_channel,
    },
    {
		/*-----CMD ID:8-----------*/
        .id    = WLNPI_CMD_GET_RSSI,
        .name  = "get_rssi",
        .help  = "get_rssi",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_rssi,
    },
    {
        /*-----CMD ID:9-----------*/
        .id    = WLNPI_CMD_SET_TX_MODE,
        .name  = "set_tx_mode",
        .help  = "set_tx_mode [0:duty cycle|1:carrier suppressioon|2:local leakage]",
        .parse = wlnpi_cmd_set_tx_mode,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:10-----------*/
        .id    = WLNPI_CMD_GET_TX_MODE,
        .name  = "get_tx_mode",
        .help  = "get_tx_mode",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_tx_mode,
    },
    {
		/*-----CMD ID:11-----------*/
        .id    = WLNPI_CMD_SET_RATE,
        .name  = "set_rate",
        .help  = "set_rate <NUM>",
        .parse = wlnpi_cmd_set_rate,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:12-----------*/
        .id    = WLNPI_CMD_GET_RATE,
        .name  = "get_rate",
        .help  = "get_rate",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_rate,
    },
    {
        /*-----CMD ID:13-----------*/
        .id    = WLNPI_CMD_SET_BAND,
        .name  = "set_band",
        .help  = "set_band <NUM>",
        .parse = wlnpi_cmd_set_band,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:14-----------*/
        .id    = WLNPI_CMD_GET_BAND,
        .name  = "get_band",
        .help  = "get_band",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_band,
    },
    {
        /*-----CMD ID:15-----------*/
        .id    = WLNPI_CMD_SET_BW,
        .name  = "set_cbw",
        .help  = "set_cbw [0:20M|1:40M|2:80M|3:160M]",
        .parse = wlnpi_cmd_set_cbw,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:16-----------*/
        .id    = WLNPI_CMD_GET_BW,
        .name  = "get_cbw",
        .help  = "get_cbw",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_cbw,
    },
	{
        /*-----CMD ID:17-----------*/
        .id    = WLNPI_CMD_SET_PKTLEN,
        .name  = "set_pkt_len",
        .help  = "set_pkt_len <NUM>",
        .parse = wlnpi_cmd_set_pkt_length,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:18-----------*/
        .id    = WLNPI_CMD_GET_PKTLEN,
        .name  = "get_pkt_len",
        .help  = "get_pkt_len",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_pkt_length,
    },
    {
        /*-----CMD ID:19-----------*/
        .id    = WLNPI_CMD_SET_PREAMBLE,
        .name  = "set_preamble",
        .help  = "set_preamble [0:normal|1:cck|2:mixed|3:11n green|4:11ac]",
        .parse = wlnpi_cmd_set_preamble,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:20-----------*/
        .id    = WLNPI_CMD_SET_GUARD_INTERVAL,
        .name  = "set_gi",
        .help  = "set_gi [0:long|1:short]",
        .parse = wlnpi_cmd_set_guard_interval,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:21-----------*/
        .id    = WLNPI_CMD_GET_GUARD_INTERVAL,
        .name  = "get_gi",
        .help  = "get_gi",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_guard_interval,
    },
        /*-----CMD ID:22-----------*/
      /*.id    = WLNPI_CMD_SET_BURST_INTERVAL,*/
        /*-----CMD ID:23-----------*/
	/*  .id    = WLNPI_CMD_GET_BURST_INTERVAL,*/
    {
        /*-----CMD ID:24-----------*/
        .id    = WLNPI_CMD_SET_PAYLOAD,
        .name  = "set_payload",
        .help  = "set_payload <NUM>",
        .parse = wlnpi_cmd_set_payload,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:25-----------*/
        .id    = WLNPI_CMD_GET_PAYLOAD,
        .name  = "get_payload",
        .help  = "get_payload",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_payload,
    },
    {
		/*-----CMD ID:26-----------*/
        .id    = WLNPI_CMD_SET_TX_POWER,
        .name  = "set_tx_power",
        .help  = "set_tx_power <NUM>",
        .parse = wlnpi_cmd_set_tx_power,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:27-----------*/
        .id    = WLNPI_CMD_GET_TX_POWER,
        .name  = "get_tx_power",
        .help  = "get_tx_power",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_tx_power,
    },
    {
		/*-----CMD ID:28-----------*/
        .id    = WLNPI_CMD_SET_TX_COUNT,
        .name  = "set_tx_count",
        .help  = "set_tx_count <NUM>",
        .parse = wlnpi_cmd_set_tx_count,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:29-----------*/
        .id    = WLNPI_CMD_GET_RX_OK_COUNT,
        .name  = "get_rx_ok",
        .help  = "get_rx_ok",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_rx_ok,
    },
    {
		/*-----CMD ID:30-----------*/
        .id    = WLNPI_CMD_TX_START,
        .name  = "tx_start",
        .help  = "tx_start",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:31-----------*/
        .id    = WLNPI_CMD_TX_STOP,
        .name  = "tx_stop",
        .help  = "tx_stop",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:32-----------*/
        .id    = WLNPI_CMD_RX_START,
        .name  = "rx_start",
        .help  = "rx_start",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:33-----------*/
        .id    = WLNPI_CMD_RX_STOP,
        .name  = "rx_stop",
        .help  = "rx_stop",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:34-----------*/
        .id    = WLNPI_CMD_GET_REG,
        .name  = "get_reg",
        .help  = "get_reg <mac/phy0/phy1/rf> <addr_offset> [count]",
        .parse = wlnpi_cmd_get_reg,
        .show  = wlnpi_show_get_reg,
    },
    {
		/*-----CMD ID:35-----------*/
        .id    = WLNPI_CMD_SET_REG,
        .name  = "set_reg",
        .help  = "set_reg <mac/phy0/phy1/rf> <addr_offset> <value>",
        .parse = wlnpi_cmd_set_reg,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:36-----------*/
        .id    = WLNPI_CMD_SIN_WAVE,
        .name  = "sin_wave",
        .help  = "sin_wave",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:37-----------*/
        .id    = WLNPI_CMD_LNA_ON,
        .name  = "lna_on",
        .help  = "lna_on",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:38-----------*/
        .id    = WLNPI_CMD_LNA_OFF,
        .name  = "lna_off",
        .help  = "lna_off",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:39-----------*/
        .id    = WLNPI_CMD_GET_LNA_STATUS,
        .name  = "lna_status",
        .help  = "lna_status",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_lna_status,
    },
    {
		/*-----CMD ID:40-----------*/
        .id    = WLNPI_CMD_SET_WLAN_CAP,
        .name  = "set_wlan_cap",
        .help  = "set_wlan_cap <NUM>",
        .parse = wlnpi_cmd_set_wlan_cap,
        .show  = wlnpi_show_only_status,
    },
    {
		/*-----CMD ID:41-----------*/
        .id    = WLNPI_CMD_GET_WLAN_CAP,
        .name  = "get_wlan_cap",
        .help  = "get_wlan_cap",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_wlan_cap,
    },
    {
        /*-----CMD ID:42-----------*/
        .id    = WLNPI_CMD_GET_CONN_AP_INFO,
        .name  = "conn_status",
        .help  = "conn_status //get current connected ap's info",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_connected_ap_info,
    },
    {
        /*-----CMD ID:42-----------*/
        .id    = WLNPI_CMD_GET_CONN_AP_INFO,
        .name  = "get_reconnect",
        .help  = "get_reconnect //get reconnect times",
        .parse = wlnpi_cmd_get_reconnect,
        .show  = wlnpi_show_reconnect,
    },
    {
        /*-----CMD ID:43-----------*/
        .id    = WLNPI_CMD_GET_MCS_INDEX,
        .name  = "mcs",
        .help  = "mcs //get mcs index in use.",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_mcs_index,
    },
    {
         /*-----CMD ID:44-----------*/
        .id    = WLNPI_CMD_SET_AUTORATE_FLAG,
        .name  = "set_ar",
        .help  = "set_ar [flag:0|1] [index:NUM] [sgi:0|1] #set auto rate flag(on or off) and index.",
        .parse = wlnpi_cmd_set_ar,
        .show  = wlnpi_show_only_status,
    },
    {
         /*-----CMD ID:45-----------*/
        .id    = WLNPI_CMD_GET_AUTORATE_FLAG,
        .name  = "get_ar",
        .help  = "get_ar(get auto rate flag(on or off) and index)",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_ar,
    },
    {
         /*-----CMD ID:46-----------*/
        .id    = WLNPI_CMD_SET_AUTORATE_PKTCNT,
        .name  = "set_ar_pktcnt",
        .help  = "set_ar_pktcnt<NUM> //set auto rate pktcnt.",
        .parse = wlnpi_cmd_set_ar_pktcnt,
        .show  = wlnpi_show_only_status,
    },
    {
         /*-----CMD ID:47-----------*/
        .id    = WLNPI_CMD_GET_AUTORATE_PKTCNT,
        .name  = "get_ar_pktcnt",
        .help  = "get_ar_pktcnt//get auto rate pktcnt.",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_ar_pktcnt,
    },
    {
         /*-----CMD ID:48-----------*/
        .id    = WLNPI_CMD_SET_AUTORATE_RETCNT,
        .name  = "set_ar_retcnt",
        .help  = "set_ar_retcnt[NUM]//ar:auto rate",
        .parse = wlnpi_cmd_set_ar_retcnt,
        .show  = wlnpi_show_only_status,
    },
    {
         /*-----CMD ID:49-----------*/
        .id    = WLNPI_CMD_GET_AUTORATE_RETCNT,
        .name  = "get_ar_retcnt",
        .help  = "get_ar_pktcnt //get auto rate retcnt.",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_get_ar_retcnt,
    },
	 {
         /*-----CMD ID:50-----------*/
        .id    = WLNPI_CMD_ROAM,
        .name  = "roam",
        .help  = "roam <channel> <mac_addr> <ssid>",
        .parse = wlnpi_cmd_roam,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:51-----------*/
        .id    = WLNPI_CMD_WMM_PARAM,
        .name  = "set_wmm",
        .help  = "set_wmm [be|bk|vi|vo] {cwmin} {cwmax} {aifs} {txop}",
        .parse = wlnpi_cmd_set_wmm,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:52-----------*/
        .id    = WLNPI_CMD_ENG_MODE,
        .name  = "set_eng_mode",
        .help  = "set_eng_mode [1:set_cca|3:set_scan][0:off|1:on]",
        .parse = wlnpi_cmd_set_eng_mode,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:65-----------*/
        .id    = WLNPI_CMD_GET_DEV_SUPPORT_CHANNEL,
        .name  = "get_sup_ch",
        .help  = "get_sup_ch",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:66-----------*/
		.id    = WLNPI_CMD_GET_PA_INFO,
		.name  = "pa_info",
		.help  = "pa_info <type - bit0:common, bit1:tx, bit2:rx>",
		.parse = wlnpi_cmd_get_pa_info,
		.show  = wlnpi_show_get_pa_infor,
	},
	{
		/*-----CMD ID:67-----------*/
		.id    = WLNPI_CMD_TX_STATUS,
		.name  = "tx_status",
		.help  = "tx_status <ctxt_id>[0:get|1:clear]",
		.parse = wlnpi_cmd_tx_status,
		.show  = wlnpi_show_tx_status,
	},
	{
		/*-----CMD ID:68-----------*/
		.id    = WLNPI_CMD_RX_STATUS,
		.name  = "rx_status",
		.help  = "rx_status <ctxt_id>[0:get |1:clear]",
		.parse = wlnpi_cmd_rx_status,
		.show  = wlnpi_show_rx_status,
	},
	{
		/*-----CMD ID:69-----------*/
		.id    = WLNPI_CMD_GET_STA_LUT_STATUS,
		.name  = "sta_lut_status",
		.help  = "sta_lut_status <ctxt_id>",
		.parse = wlnpi_cmd_get_sta_lut_status,
		.show  = wlnpi_show_get_sta_lut_status,
	},
    {
        /*-----CMD ID:70-----------*/
        .id    = WLNPI_CMD_START_PKT_LOG,
        .name  = "start_pkt_log",
        .help  = "start_pkt_log [0-255] 0",
        .parse = wlnpi_cmd_start_pkt_log,
        .show  = wlnpi_show_only_status,
    },
    {
        /*-----CMD ID:71-----------*/
        .id    = WLNPI_CMD_STOP_PKT_LOG,
        .name  = "stop_pkt_log",
        .help  = "stop_pkt_log",
        .parse = wlnpi_cmd_no_argv,
        .show  = wlnpi_show_only_status,
    },
	{
		/*-----CMD ID:73-----------*/
		.id    = WLNPI_CMD_GET_QMU_STATUS,
		.name  = "get_qmu_status",
		.help  = "get_qmu_status",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_qmu_status,
	},
	{
		/*-----CMD ID:78-----------*/
		.id    = WLNPI_CMD_SET_CHAIN,
		.name  = "set_chain",
		.help  = "set_chain [1:primary CH|2:Sec CH|3:MIMO]",
		.parse = wlnpi_cmd_set_chain,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:79-----------*/
		.id    = WLNPI_CMD_GET_CHAIN,
		.name  = "get_chain",
		.help  = "get_chain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_chain,
	},
	{
		/*-----CMD ID:80-----------*/
		.id    = WLNPI_CMD_SET_SBW,
		.name  = "set_sbw",
		.help  = "set_sbw [0:20M|1:40M|2:80M|3:160M]",
		.parse = wlnpi_cmd_set_sbw,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:81-----------*/
		.id    = WLNPI_CMD_GET_SBW,
		.name  = "get_sbw",
		.help  = "get_sbw",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_sbw,
	},
	{
		/*-----CMD ID:82-----------*/
		.id    = WLNPI_CMD_SET_AMPDU_ENABLE,
		.name  = "set_ampdu_en",
		.help  = "set_ampdu_en",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:83-----------*/
		.id    = WLNPI_CMD_GET_AMPDU_ENABLE,
		.name  = "get_ampdu_en",
		.help  = "get_ampdu_en",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:84-----------*/
		.id    = WLNPI_CMD_SET_AMPDU_COUNT,
		.name  = "set_ampdu_cnt",
		.help  = "set_ampdu_cnt [0:disable NPI force|1:tx|2:rx], [0-64:ampdu_cnt], [0-3:amsdu_cnt]",
		.parse = wlnpi_cmd_set_ampdu_cnt,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:85-----------*/
		.id    = WLNPI_CMD_GET_AMPDU_COUNT,
		.name  = "get_ampdu_cnt",
		.help  = "get_ampdu_cnt",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:86-----------*/
		.id    = WLNPI_CMD_SET_STBC,
		.name  = "set_stbc",
		.help  = "set_stbc",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:87-----------*/
		.id    = WLNPI_CMD_GET_STBC,
		.name  = "get_stbc",
		.help  = "get_stbc",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:88-----------*/
		.id    = WLNPI_CMD_SET_FEC,
		.name  = "set_fec",
		.help  = "set_fec [0:BCC|1:LDPC]",
		.parse = wlnpi_cmd_set_fec,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:89-----------*/
		.id    = WLNPI_CMD_GET_FEC,
		.name  = "get_fec",
		.help  = "get_fec",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_fec,
	},
	{
		/*-----CMD ID:90-----------*/
		.id    = WLNPI_CMD_GET_RX_SNR,
		.name  = "get_rx_snr",
		.help  = "get_rx_snr",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:91-----------*/
		.id    = WLNPI_CMD_SET_FORCE_TXGAIN,
		.name  = "set_force_txgain",
		.help  = "set_force_txgain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:92-----------*/
		.id    = WLNPI_CMD_SET_FORCE_RXGAIN,
		.name  = "set_force_rxgain",
		.help  = "set_force_rxgain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:93-----------*/
		.id    = WLNPI_CMD_SET_BFEE_ENABLE,
		.name  = "set_bfee_en",
		.help  = "set_bfee_en",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:94-----------*/
		.id    = WLNPI_CMD_SET_DPD_ENABLE,
		.name  = "set_dpd_en",
		.help  = "set_dpd_en [param: 0: 2G DPD Disable|1: 2G DPD Enable]",
		.parse = wlnpi_cmd_set_dpd_enable,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:95-----------*/
		.id    = WLNPI_CMD_SET_AGC_LOG_ENABLE,
		.name  = "set_agc_log_en",
		.help  = "set_agc_log_en",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:96-----------*/
		.id    = WLNPI_CMD_SET_EFUSE,
		.name  = "set_efuse",
		.help  = "set_efuse index value",
		.parse = wlnpi_cmd_set_efuse,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:97-----------*/
		.id    = WLNPI_CMD_GET_EFUSE,
		.name  = "get_efuse",
		.help  = "get_efuse index",
		.parse = wlnpi_cmd_get_efuse,
		.show  = wlnpi_show_get_efuse,
	},
	{
		/*-----CMD ID:98-----------*/
		.id    = WLNPI_CMD_SET_TXS_CALIB_RESULT,
		.name  = "set_txs_cal_res",
		.help  = "set_txs_cal_res",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:99-----------*/
		.id    = WLNPI_CMD_GET_TXS_TEMPERATURE,
		.name  = "get_txs_temp",
		.help  = "get_txs_temp",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:100-----------*/
		.id    = WLNPI_CMD_SET_CBANK_REG,
		.name  = "set_cbank_reg",
		.help  = "set_cbank_reg",
		.parse = wlnpi_cmd_set_cbank_reg,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:101-----------*/
		.id    = WLNPI_CMD_SET_11N_GREEN_FIELD,
		.name  = "set_11n_green",
		.help  = "set_11n_green",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:102-----------*/
		.id    = WLNPI_CMD_GET_11N_GREEN_FIELD,
		.name  = "get_11n_green",
		.help  = "get_11n_green",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:103-----------*/
		.id    = WLNPI_CMD_ATENNA_COUPLING,
		.name  = "atenna_coup",
		.help  = "atenna_coup",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:104-----------*/
		.id    = WLNPI_CMD_SET_FIX_TX_RF_GAIN,
		.name  = "set_tx_rf_gain",
		.help  = "set_tx_rf_gain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:105-----------*/
		.id    = WLNPI_CMD_SET_FIX_TX_PA_BIAS,
		.name  = "set_tx_pa_bias",
		.help  = "set_tx_pa_bias",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:106-----------*/
		.id    = WLNPI_CMD_SET_FIX_TX_DVGA_GAIN,
		.name  = "set_tx_dvga_gain",
		.help  = "set_tx_dvga_gain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:107-----------*/
		.id    = WLNPI_CMD_SET_FIX_RX_LNA_GAIN,
		.name  = "set_rx_lna_gain",
		.help  = "set_rx_lna_gain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:108-----------*/
		.id    = WLNPI_CMD_SET_FIX_RX_VGA_GAIN,
		.name  = "set_rx_vga_gain",
		.help  = "set_rx_vga_gain",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:109-----------*/
		.id    = WLNPI_CMD_SET_MAC_BSSID,
		.name  = "set_mac_bssid",
		.help  = "set_mac_bssid",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:110-----------*/
		.id    = WLNPI_CMD_GET_MAC_BSSID,
		.name  = "get_mac_bssid",
		.help  = "get_mac_bssid",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:111-----------*/
		.id    = WLNPI_CMD_FORCE_TX_RATE,
		.name  = "force_tx_rate",
		.help  = "force_tx_rate",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:112-----------*/
		.id    = WLNPI_CMD_FORCE_TX_POWER,
		.name  = "force_tx_power",
		.help  = "force_tx_power",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:113-----------*/
		.id    = WLNPI_CMD_ENABLE_FW_LOOP_BACK,
		.name  = "en_fw_loop",
		.help  = "en_fw_loop",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:115-----------*/
		.id    = WLNPI_CMD_SET_PROTECTION_MODE,
		.name  = "set_prot_mode",
		.help  = "set_prot_mode [mode:1:sta|2:ap|4:p2p_dev|5:p2p_cli|6:p2p_go|7:ibss] [value]",
		.parse = wlnpi_cmd_set_prot_mode,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:116-----------*/
		.id    = WLNPI_CMD_GET_PROTECTION_MODE,
		.name  = "get_prot_mode",
		.help  = "get_prot_mode [mode:1:sta|2:ap|4:p2p_dev|5:p2p_cli|6:p2p_go|7:ibss]",
		.parse = wlnpi_cmd_get_prot_mode,
		.show  = wlnpi_show_get_prot_mode,
	},
	{
		/*-----CMD ID:117-----------*/
		.id    = WLNPI_CMD_SET_RTS_THRESHOLD,
		.name  = "set_threshold",
		.help  = "set_threshold [mode:1:sta|2:ap|4:p2p_dev|5:p2p_cli|6:p2p_go|7:ibss] [rts:] [flag:]",
		.parse = wlnpi_cmd_set_threshold,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:118-----------*/
		.id    = WLNPI_CMD_SET_AUTORATE_DEBUG,
		.name  = "set_ar_dbg",
		.help  = "set_ar_dbg [id:0:retry_limt|1:update_st|3:undefine] [hiq_retry_l] [hiq_retry_s] [ampdu_retry]",
		.parse = wlnpi_cmd_set_ar_dbg,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:119-----------*/
		.id    = WLNPI_CMD_SET_PM_CTL,
		.name  = "set_pm_ctl",
		.help  = "set_pm_ctl [int32]",
		.parse = wlnpi_cmd_set_pm_ctl,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:120-----------*/
		.id    = WLNPI_CMD_GET_PM_CTL,
		.name  = "get_pm_ctl",
		.help  = "get_pm_ctl",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_pm_ctl,
	},
	{
		/*-----CMD ID:121-----------*/
		.id    = WLNPI_CMD_SET_MAC_EFUSE,
		.name  = "set_mac_efuse",
		.help  = "set_mac_efuse <xx:xx:xx:xx:xx:xx>",
		.parse = wlnpi_cmd_set_mac_efuse,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:122-----------*/
		.id    = WLNPI_CMD_GET_MAC_EFUSE,
		.name  = "get_mac_efuse",
		.help  = "get_mac_efuse",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_mac_efuse,
	},
	{
		/*-----CMD ID:123-----------*/
		.id    = WLNPI_CMD_GET_RF_CONFIG,
		.name  = "get_rf_config",
		.help  = "get_rf_config",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_rf_config,
	},
	{
		/*-----CMD ID:124-----------*/
		.id    = WLNPI_CMD_SET_TSSI,
		.name  = "set_tssi",
		.help  = "set_tssi value",
		.parse = wlnpi_cmd_set_tssi,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:125-----------*/
		.id    = WLNPI_CMD_GET_TSSI,
		.name  = "get_tssi",
		.help  = "get_tssi",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_tssi,
	},
	{
		/*-----CMD ID:126-----------*/
		.id    = WLNPI_CMD_SET_CCA_TH,
		.name  = "set_cca_th",
		.help  = "set_cca_th value",
		.parse = wlnpi_cmd_set_cca_th,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:127-----------*/
		.id    = WLNPI_CMD_RESTORE_CCA_TH,
		.name  = "restore_cca_th",
		.help  = "restore_cca_th",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:128-----------*/
		.id    = WLNPI_CMD_SET_BEAMFORMING_STATUS,
		.name  = "set_beamf_status",
		.help  = "set_beamf_status [status: 0:disable|1:enable]",
		.parse = wlnpi_cmd_set_beamforming_status,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:129-----------*/
		.id    = WLNPI_CMD_SET_RXSTBC_STATUS,
		.name  = "set_rxstbc_status",
		.help  = "set_rxstbc_status [status: 0:disable|1:enable]",
		.parse = wlnpi_cmd_set_rxstbc_status,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:130-----------*/
		.id    = WLNPI_CMD_SET_LTE_AVOID,
		.name  = "set_lte_avoid",
		.help  = "set_lte_avoid [context-id] [user-channel: 0:automatically | [1-14]:user channel]",
		.parse = wlnpi_cmd_set_lte_avoid,
		.show  = wlnpi_show_only_status,
	},
	{
		/*-----CMD ID:131-----------*/
		.id    = WLNPI_CMD_GET_DPD_COEFFI,
		.name  = "get_dpd_coeffi",
		.help  = "get_dpd_coeffi [chain: 1:Primary|2:Diversity] [lut_index: 0|1|2]",
		.parse = wlnpi_cmd_get_dpd_coeffi,
		.show  = wlnpi_show_dpd_coeffi,
	},
	{
		/*-----CMD ID:136-----------*/
		.id    = WLNPI_CMD_GET_CHIPID,
		.name  = "get_chip_id",
		.help  = "get_chip_id",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_chip_id,
	},
	{
		/*-----CMD ID:138-----------*/
		.id    = WLNPI_CMD_GET_BEAMFORMING_STATUS,
		.name  = "get_beamf_status",
		.help  = "get_beamf_status [status: 0:disable|1:enable]",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_beamforming_status,
	},
	{
		/*-----CMD ID:139-----------*/
		.id    = WLNPI_CMD_GET_RXSTBC_STATUS,
		.name  = "get_rxstbc_status",
		.help  = "get_rxstbc_status [status: 0:disable|1:enable]",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_rxstbc_status,
	},
	{
		/*----CMD ID:147------------*/
		.id    = WLNPI_CMD_SET_SAR_POWER,
		.name  = "set_sar_power",
		.help  = "set_sar_power [type] [power] [mode]",
		.parse = wlnpi_cmd_set_sar_power,
		.show  = wlnpi_show_only_status,
	},
	{
		/*----CMD ID:148------------*/
		.id    = WLNPI_CMD_GET_SAR_POWER,
		.name  = "get_sar_power",
		.help  = "get_sar_power",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_show_get_sar_power,
	},
	{
		/*----CMD ID:149------------*/
		.id    = WLNPI_CMD_SET_INI_REG_EN,
		.name  = "set_ini_reg_en",
		.help  = "set_ini_reg_en [enable: 0:disable|1:enable]",
		.parse = wlnpi_cmd_set_ini_reg_en,
		.show  = wlnpi_show_only_status,
	},
	{
		/*----CMD ID:154------------*/
		.id    = WLNPI_CMD_SET_SOFTAP_WFA_PARA,
		.name  = "set_softap_wfa",
		.help  = "set_softap_wfa [1:pmf|2:wmm|3~15:reserved]",
		.parse = wlnpi_cmd_set_softap_wfa_para,
		.show  = wlnpi_show_only_status,
	},
	{
		 /*----CMD ID:155------------*/
		.id    = WLNPI_CMD_GET_SOFTAP_WFA_PARA,
		.name  = "get_softap_wfa",
		.help  = "get_softap_wfa",
		.parse = wlnpi_cmd_no_argv,
		.show = wlnpi_cmd_get_softap_wfa_para,
	},
	{
		 /*----CMD ID:156------------*/
		.id    = WLNPI_CMD_SET_STA_WFA_PARA,
		.name  = "set_sta_wfa",
		.help  = "set_sta_wfa [BIT0:11r|BIT1:11k|BIT2:VMM-AC|BIT3:11u qos map set|BIT4:11n vmm]",
		.parse = wlnpi_cmd_set_sta_wfa_para,
		.show  = wlnpi_show_only_status,
	},
	{
		 /*----CMD ID:157------------*/
		.id    = WLNPI_CMD_GET_STA_WFA_PARA,
		.name  = "get_sta_wfa",
		.help  = "get_sta_wfa",
		.parse = wlnpi_cmd_no_argv,
		.show  = wlnpi_cmd_get_sta_wfa_para,
	},
	{
        /*CMD ID:200*/
        .id   = WLNPI_CMD_SET_RAND_MAC_FLAG,
        .name  = "set_rand_mac_flag",
        .help  = "set_rand_mac_flag",
        .parse = wlnpi_cmd_set_rand_mac_flag,
        .show  = wlnpi_show_only_status,
    }
};

struct wlnpi_cmd_t *match_cmd_table(char *name)
{
    size_t          i;
    struct wlnpi_cmd_t *cmd = NULL;
    for(i=0; i< sizeof(g_cmd_table)/sizeof(struct wlnpi_cmd_t) ; i++)
    {
        if( 0 != strcmp(name, g_cmd_table[i].name) )
        {
            continue;
        }

        cmd = &g_cmd_table[i];
        break;
    }
    return cmd;
}

void do_help(void)
{
    int i, max;
    max = sizeof(g_cmd_table)/sizeof(struct wlnpi_cmd_t);
    for(i=0; i<max; i++)
    {
        printf("iwnpi wlan0 %s\n", g_cmd_table[i].help);
    }
    return;
}

/********************************************************************
*   name   iwnpi_hex_dump
*   ---------------------------
*   description:
*   ----------------------------
*   para        IN/OUT      type            note
*   ----------------------------------------------------
*   return
*   0:exec successful
*   -1:error
*   ------------------
*   other:
*
********************************************************************/
int iwnpi_hex_dump(unsigned char *name, unsigned short nLen, unsigned char *pData, unsigned short len)
{
    char *str;
    int i, p, ret;

    ENG_LOG("ADL entry %s(), len = %d", __func__, len);

    if (len > 1024)
        len = 1024;
    str = malloc(((len + 1) * 3 + nLen));
    if (str == NULL) {
	    printf("%s() malloc fail", __func__);
	    return -1;
    }
    memset(str, 0, (len + 1) * 3 + nLen);
    memcpy(str, name, nLen);
    if ((NULL == pData) || (0 == len))
    {
        printf("%s\n", str);
        free(str);
        return 0;
    }

    p = 0;
    for (i = 0; i < len; i++)
    {
        ret = sprintf((str + nLen + p), "%02x ", *(pData + i));
        p = p + ret;
    }
    ENG_LOG("%s\n\n", str);
    free(str);

    ENG_LOG("ADL levaling %s()", __func__);
    return 0;
}
