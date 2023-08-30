# This file has been modified by Unisoc(Shanghai)Technologies Co.,Ltd in 2023.

CONNECTIVITY_OWN_FILES := \
    bt_configure_rf.ini \
    bt_configure_pskey.ini \
    bt_configure_rf_aa.ini \
    bt_configure_pskey_aa.ini \
    fm_board_config.ini

WIFI_INI_FILES := \
    wifi_board_config.ini\
    wifi_board_config_aa.ini\

BOARD_HAVE_SPRD_WCN_BRANCH ?= marlin3_20a

SPRD_WCN_ETC_PATH ?= $(TARGET_COPY_OUT_ODM)/firmware
SPRD_WIFI_FIRMWARE_PATH := $(TARGET_COPY_OUT_ODM)/firmware

SPRD_WIFI_ODM_ETC_PATH :=$(TARGET_COPY_OUT_ODM)/etc
SPRD_WIFI_ODM_FIRMWARE_PATH :=$(TARGET_COPY_OUT_ODM)/firmware

CONNECTIVITY_FM_FILES := fm_board_config.ini
SPRD_WCN_FM_PATH := $(TARGET_COPY_OUT_ODM)/firmware

SPRD_WCN_FIRMWARE_FILES := \
    wcnmodem.bin\
    gnssmodem.bin

SPRD_WCN_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/$(BOARD_HAVE_SPRD_WCN_BRANCH)/sc2355_marlin3_lite_ab_builddir/EXEC_KERNEL_IMAGE.bin
SPRD_GNSS_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20b_new/marlin3lite/marlin3lite_gnss_cm4_builddir/gnssmodem.bin

GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(CONNECTIVITY_OWN_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WCN_ETC_PATH)/$(own), \
        $(error wcn chip ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

#move fm ini to odm to use request_firmware in driver
GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(CONNECTIVITY_FM_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WCN_FM_PATH)/$(own), \
        $(error wcn chip fm ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

#move wifi ini to odm to use request_firmware in driver
GENERATE_WIFI_INI_ODM_FIRMWARE_FILES += $(foreach own, $(WIFI_INI_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_ODM_FIRMWARE_PATH)/$(own), \
        $(error wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

GENERATE_WIFI_INI_ODM_ETC_FILES += $(foreach own, $(WIFI_INI_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_ODM_ETC_PATH)/$(own), \
        $(error wcn wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don' t take a random one)))

ifneq ($(wildcard $(SPRD_WCN_MODEM_FIRMWARE)), )
    GENERATE_WCN_PRODUCT_COPY_FILES += \
        $(SPRD_WCN_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/wcnmodem.bin \
        $(SPRD_GNSS_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/gnssmodem.bin
else
    GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(SPRD_WCN_FIRMWARE_FILES), \
        $(if $(wildcard $(LOCAL_PATH)/$(own)), \
            $(LOCAL_PATH)/$(own):$(SPRD_WIFI_FIRMWARE_PATH)/$(own), \
            $(error wcn firmware bin $(own) miss. please fix it, and don't take a random one)))
endif

VER_BTWF=vendor/sprd/release/unisoc_bin/$(BOARD_HAVE_SPRD_WCN_BRANCH)/version.txt
VER_GNSS=vendor/sprd/release/unisoc_bin/gnss_20b_new/marlin3lite/version.txt

PRODUCT_COPY_FILES += \
    $(GENERATE_WCN_PRODUCT_COPY_FILES) \
    $(GENERATE_WIFI_INI_ODM_FIRMWARE_FILES) \
    $(GENERATE_WIFI_INI_ODM_ETC_FILES) \
    $(LOCAL_PATH)/wcn.rc:/$(TARGET_COPY_OUT_ODM)/etc/init/wcn.rc \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:vendor/etc/permissions/android.hardware.bluetooth_le.xml

PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.wcn.hardware.etcpath=/$(SPRD_WCN_ETC_PATH) \
    ro.bt.bdaddr_path="/data/vendor/bluetooth/btmac.txt" \
    persist.bluetooth.a2dp_offload.cap = "sbc" \
    persist.bluetooth.a2dp_offload.switch = "false"

PRODUCT_ODM_PROPERTIES += \
    ro.vendor.wcn.hardware.product=$(SPRD_WCN_HW_MODEL) \
    persist.bluetooth.a2dp_offload.disabled = "true" \
    ro.bluetooth.a2dp_offload.supported="false"

ifeq ($(SPRD_WCN_HW_CONFIG), $(filter ums9620_2h10 ums9230_1h10 ums9230_4h10 ums9621_1h10,$(SPRD_WCN_HW_CONFIG)))
    PRODUCT_ODM_PROPERTIES += \
        ro.vendor.enable.chr="true"
endif
