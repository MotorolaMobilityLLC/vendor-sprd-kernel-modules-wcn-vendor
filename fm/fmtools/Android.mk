LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        main.c

LOCAL_VENDOR_MODULE := true

LOCAL_MODULE := fm_tools
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

