CONNECTIVITY_OWN_FILES := \
    connectivity_calibration.ini \
    connectivity_configure.ini

CONNECTIVITY_FIRMWARE_FILES := \
    connectivity_calibration.ini \
    connectivity_configure.ini \
    connectivity_calibration.ad.ini \
    connectivity_configure.ad.ini

SUFFIX_AD_NAME := .ad.ini
SPRD_WCN_ETC_PATH ?= $(TARGET_COPY_OUT_ODM)/etc
SPRD_WIFI_FIRMWARE_PATH := $(TARGET_COPY_OUT_ODM)/firmware

SPRD_WCN_FIRMWARE_FILES := \
    wcnmodem.bin\
    gpsbd.bin\
    gpsgl.bin

SPRD_WCN_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/marlin2_18a/sharkle_cm4_v2_builddir/PM_sharkle_cm4_v2.bin
SPRD_GNSS_BD_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20a/bds_gal_integration_builddir/gnssbdmodem_integration.bin
SPRD_GNSS_GL_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20a/glo_gal_integration_builddir/gnssmodem_integration.bin

SPRD_WCN_ETC_AD_PATH := $(SPRD_WCN_ETC_PATH)/wcn

GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(CONNECTIVITY_OWN_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WCN_ETC_PATH)/$(own), \
        $(error wcn chip ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

GENERATE_WCN_PRODUCT_COPY_AD_FILE += $(foreach own, $(CONNECTIVITY_OWN_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(addsuffix $(SUFFIX_AD_NAME), $(basename $(own)))), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(addsuffix $(SUFFIX_AD_NAME), $(basename $(own))):$(SPRD_WCN_ETC_AD_PATH)/$(own), \
        $(error wcn chip ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

#copy wifi ini to vendor/firmware to use request_firmware in driver
GENERATE_WIFI_INI_COPY_FILES += $(foreach own, $(CONNECTIVITY_FIRMWARE_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_FIRMWARE_PATH)/$(own), \
        $(error wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

ifneq ($(wildcard $(SPRD_WCN_MODEM_FIRMWARE)), )
    GENERATE_WCN_PRODUCT_COPY_FILES += \
        $(SPRD_WCN_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/wcnmodem.bin \
        $(SPRD_GNSS_BD_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/gpsbd.bin \
        $(SPRD_GNSS_GL_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/gpsgl.bin
else
    GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(SPRD_WCN_FIRMWARE_FILES), \
        $(if $(wildcard $(LOCAL_PATH)/$(own)), \
            $(LOCAL_PATH)/$(own):$(SPRD_WIFI_FIRMWARE_PATH)/$(own), \
            $(error wcn firmware bin $(own) miss. please fix it, and don't take a random one)))
endif

VER_BTWF=vendor/sprd/release/unisoc_bin/marlin2_18a/version.txt
VER_GNSS=vendor/sprd/release/unisoc_bin/gnss_20a/version.txt

PRODUCT_COPY_FILES += \
    $(GENERATE_WCN_PRODUCT_COPY_FILES) \
    $(GENERATE_WCN_PRODUCT_COPY_AD_FILE) \
    $(GENERATE_WIFI_INI_COPY_FILES) \
    $(LOCAL_PATH)/wcn.rc:/$(TARGET_COPY_OUT_ODM)/etc/init/wcn.rc \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:vendor/etc/permissions/android.hardware.bluetooth_le.xml

PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.wcn.hardware.product=$(SPRD_WCN_HW_MODEL) \
    ro.vendor.wcn.hardware.etcpath=/$(SPRD_WCN_ETC_PATH) \
    ro.bt.bdaddr_path="/data/vendor/bluetooth/btmac.txt"
