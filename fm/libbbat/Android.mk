LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_32_BIT_ONLY := true

LOCAL_SRC_FILES := fm.cpp \
		   audio.cpp \
		   fmtest.cpp

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := autotestfm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := npidevice
LOCAL_CFLAGS += -DGOOGLE_FM_INCLUDED

LOCAL_C_INCLUDES:= \
    $(TOP)/vendor/sprd/tools/engpc/sprd_fts_inc \
        vendor/sprd/modules/wcn/vendor/bt/libengbt

LOCAL_SHARED_LIBRARIES:= \
             libcutils \
	     libutils \
             liblog  \
	     libhardware \
             libhardware_legacy

include $(BUILD_SHARED_LIBRARY)
