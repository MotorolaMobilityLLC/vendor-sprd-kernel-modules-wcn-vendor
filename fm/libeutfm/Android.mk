ifeq ($(filter $(strip $(PLATFORM_VERSION)),S 12),)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_32_BIT_ONLY := true

LOCAL_SRC_FILES := fmeut.c

LOCAL_MODULE := libfm-sprd_eut
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := npidevice

LOCAL_C_INCLUDES:= \
	$(TOP)/vendor/sprd/tools/engpc/sprd_fts_inc \
	$(LOCAL_PATH)/include \

LOCAL_SHARED_LIBRARIES:= liblog libc libcutils libtracedump libutils
#ifeq (1, $(strip $(shell expr $(ANDROID_MAJOR_VER) \>= 8)))
LOCAL_PROPRIETARY_MODULE := true
#endif
include $(BUILD_SHARED_LIBRARY)

endif
