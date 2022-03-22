LOCAL_PATH := $(call my-dir)

#ifneq ($(BOARD_HAVE_WCN_UNISOC),)

include $(CLEAR_VARS)

LOCAL_CFLAGS += \
        -Wall \
        -Werror \
        -Wno-switch \
        -Wno-unused-function \
        -Wno-unused-parameter \
        -Wno-unused-variable \

LOCAL_SRC_FILES := \
        wcn_vendor.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES := libutils_headers

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog

LOCAL_MODULE := libwcn-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := unisoc
#/vendor/lib/libwcn-vendor.so
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

#endif # BOARD_HAVE_WCN_UNISOC
