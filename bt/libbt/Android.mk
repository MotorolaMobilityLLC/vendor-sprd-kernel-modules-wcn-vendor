LOCAL_PATH := $(call my-dir)

ifneq ($(SPRD_WCNBT_CHISET),)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        src/bt_vendor_sprd.c \
        src/userial_vendor.c \
        src/bt_vendor_sprd_ssp.c \
        src/bt_vendor_sprd_hci.c \
        src/hardware.c \
        src/upio.c \
        src/conf.c \
        src/sitm.c \
        conf/sprd/$(SPRD_WCNBT_CHISET)/src/$(SPRD_WCNBT_CHISET).c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/conf/sprd/$(SPRD_WCNBT_CHISET)/include


LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog

## Special configuration ##
ifeq ($(BOARD_SPRD_WCNBT_MARLIN), true)
    ifneq ($(strip $(WCN_EXTENSION)),true)
        LIBBT_CFLAGS += -DSPRD_WCNBT_MARLIN_15A
    else
        LIBBT_CFLAGS += -DGET_MARLIN_CHIPID
    endif
endif


## Compatible with different osi
ifeq ($(PLATFORM_VERSION),6.0)
    LIBBT_CFLAGS += -DOSI_COMPAT_ANROID_6_0
else
#    LOCAL_SRC_FILES += ../hcidump/btsnoop_sprd_for_raw.c
endif


ifneq ($(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR),)
  bdroid_C_INCLUDES := $(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR)
  bdroid_CFLAGS += -DHAS_BDROID_BUILDCFG
else
  bdroid_C_INCLUDES :=
  bdroid_CFLAGS += -DHAS_NO_BDROID_BUILDCFG
endif

LOCAL_C_INCLUDES += $(bdroid_C_INCLUDES)
LOCAL_CFLAGS := $(LIBBT_CFLAGS) $(bdroid_CFLAGS)
LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := sprd
LOCAL_PROPRIETARY_MODULE := true

ifeq ($(PLATFORM_VERSION),4.4.4)
include $(LOCAL_PATH)/vnd_buildcfg_4_4.mk
else
include $(LOCAL_PATH)/vnd_buildcfg.mk
endif

include $(BUILD_SHARED_LIBRARY)

endif
