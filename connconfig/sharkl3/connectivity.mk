CONNECTIVITY_OWN_FILES := \
    connectivity_calibration.ini \
    connectivity_configure.ini

CONNECTIVITY_22NM_FILES := \
    connectivity_calibration.ini \
    connectivity_configure.ini \
    connectivity_calibration.22nm.ini \
    connectivity_configure.22nm.ini

SUFFIX_22NM_NAME := .22nm.ini
SPRD_WCN_ETC_PATH ?= $(TARGET_COPY_OUT_ODM)/etc
SPRD_WIFI_FIRMWARE_PATH := $(TARGET_COPY_OUT_ODM)/firmware

SPRD_WIFI_ODM_ETC_PATH := $(TARGET_COPY_OUT_ODM)/etc
SPRD_WIFI_ODM_FIRMWARE_PATH := $(TARGET_COPY_OUT_ODM)/firmware

SPRD_WCN_FIRMWARE_FILES := \
    wcnmodem.bin\
    gpsbd.bin\
    gpsgl.bin

SPRD_WCN_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/marlin2_18a/9863_integration_cm4_v2_builddir/9863_integration_cm4_v2.bin
SPRD_GNSS_BD_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20a/bds_gal_integration_builddir/PM_greeneye2_cm4_integration_bd.bin
SPRD_GNSS_GL_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20a/glo_gal_integration_builddir/PM_greeneye2_cm4_integration_glo.bin

GENERATE_WIFI_INI_ODM_ETC_FILES += $(foreach own, $(CONNECTIVITY_22NM_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_ODM_ETC_PATH)/$(own), \
        $(error wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

#copy wifi ini to odm/firmware to use request_firmware in driver
GENERATE_WIFI_INI_ODM_FIRMWARE_FILES += $(foreach own, $(CONNECTIVITY_22NM_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_ODM_FIRMWARE_PATH)/$(own), \
        $(error wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))

ifneq ($(wildcard $(SPRD_WCN_MODEM_FIRMWARE)), )
    GENERATE_WCN_PRODUCT_COPY_FILES += \
        $(SPRD_WCN_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/wcnmodem.bin \
        $(SPRD_GNSS_BD_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/gpsbd.bin \
        $(SPRD_GNSS_GL_MODEM_FIRMWARE):$(SPRD_WIFI_FIRMWARE_PATH)/gpsgl.bin \

else
    GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(SPRD_WCN_FIRMWARE_FILES), \
        $(if $(wildcard $(LOCAL_PATH)/$(own)), \
            $(LOCAL_PATH)/$(own):$(SPRD_WIFI_FIRMWARE_PATH)/$(own), \
            $(error wcn firmware bin $(own) miss. please fix it, and don't take a random one)))
endif

VER_BTWF=vendor/sprd/release/unisoc_bin/marlin2_18a/9863_integration_cm4_v2_builddir/version.txt
VER_GNSS=vendor/sprd/release/unisoc_bin/gnss_20a/bds_gal_integration_builddir/version.txt

PRODUCT_COPY_FILES += \
    $(GENERATE_WCN_PRODUCT_COPY_FILES) \
    $(GENERATE_WIFI_INI_ODM_ETC_FILES) \
    $(GENERATE_WIFI_INI_ODM_FIRMWARE_FILES) \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:vendor/etc/permissions/android.hardware.bluetooth_le.xml \
    $(LOCAL_PATH)/wcn.rc:/vendor/etc/init/wcn.rc

PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.wcn.hardware.etcpath=/$(SPRD_WCN_ETC_PATH) \
    persist.bluetooth.a2dp_offload.aidl_flag="aidl" \
    ro.bt.bdaddr_path="/data/vendor/bluetooth/btmac.txt"

PRODUCT_ODM_PROPERTIES += \
    ro.vendor.wcn.hardware.product=$(SPRD_WCN_HW_MODEL)
