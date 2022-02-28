#add 2355 npi app
LOCAL_PATH := $(call my-dir)
ifeq ($(strip $(BOARD_WLAN_DEVICE)),sc2355)
#source files
WLNPI_OBJS	  += wlnpi.c cmd2355.c
#cflags
IWNPI_CFLAGS ?= -O2 -g
IWNPI_CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -DCONFIG_LIBNL20

#Build wlnpi tool
include $(CLEAR_VARS)
LOCAL_MODULE := iwnpi
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES += libcutils   \
                          libutils    \
			  liblog      \
			  libnl

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS = $(IWNPI_CFLAGS)
LOCAL_SRC_FILES = $(WLNPI_OBJS)
include $(BUILD_EXECUTABLE)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
