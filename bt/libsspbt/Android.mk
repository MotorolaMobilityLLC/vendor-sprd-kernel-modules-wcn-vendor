LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        p_256_ecc_pp.c \
        p_256_curvepara.c \
        p_256_multprecision.c \
        lmp_ecc.c \
        algo_api.c \
        algo_utils.c \
        bt_vendor_ssp_lib.c

LOCAL_C_INCLUDES +=

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog

LOCAL_MODULE := libbt-ssp_bt
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := sprd
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
