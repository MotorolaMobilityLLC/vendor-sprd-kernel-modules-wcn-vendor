$(warning shell echo "build liblte entry")

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH), $(filter $(TARGET_ARCH), arm arm64))
LIB_PLATFORM := arm
else ifeq ($(TARGET_ARCH), $(filter $(TARGET_ARCH), x86 x86_64))
LIB_PLATFORM := x86
endif

LOCAL_MODULE := liblte
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE).so
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE).so

LOCAL_SRC_FILES_32 := $(LIB_PLATFORM)/32bit/liblte.so
LOCAL_SRC_FILES_64 := $(LIB_PLATFORM)/64bit/liblte.so

LOCAL_MODULE_TAGS := optional

LOCAL_VENDOR_MODULE := true

LOCAL_SHARED_LIBRARIES := libc++ libc libcutils libdl liblog libm libpower libui libutils

include $(BUILD_PREBUILT)

$(warning shell echo "build liblte end")
