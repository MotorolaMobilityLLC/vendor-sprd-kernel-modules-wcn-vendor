CONNECTIVITY_OWN_FILES := \
    bt_configure_rf.ini \
    bt_configure_pskey.ini \
    fm_board_config.ini \
    wifi_board_config.ini

WIFI_INI_FILES := \
    wifi_board_config.ini\

SPRD_WCN_ETC_PATH ?= vendor/etc
SPRD_WIFI_FIRMWARE_PATH := vendor/firmware

SPRD_WCN_FIRMWARE_FILES := \
    wcnmodem.bin\
    gnssmodem.bin

SPRD_WCN_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/wcn_trunk/umw2631_qogirL6_builddir/PM_umw2631_qogirL6.bin
SPRD_GNSS_MODEM_FIRMWARE := vendor/sprd/release/unisoc_bin/gnss_20b_new/qogirl6/QogirL6_gnss_cm4_builddir/PM_QogirL6_gnss_cm4.bin

GENERATE_WCN_PRODUCT_COPY_FILES += $(foreach own, $(CONNECTIVITY_OWN_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WCN_ETC_PATH)/$(own), \
        $(LOCAL_PATH)/default/$(own):$(SPRD_WCN_ETC_PATH)/$(own)))

#copy wifi ini to vendor/firmware to use request_firmware in driver
GENERATE_WIFI_INI_COPY_FILES += $(foreach own, $(WIFI_INI_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own)), \
        $(LOCAL_PATH)/$(SPRD_WCN_HW_CONFIG)/$(own):$(SPRD_WIFI_FIRMWARE_PATH)/$(own), \
        $(error wifi ini $(SPRD_WCN_HW_CONFIG) $(own) miss. please fix it, and don't take a random one)))


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

PRODUCT_COPY_FILES += \
    $(GENERATE_WCN_PRODUCT_COPY_FILES) \
	$(GENERATE_WIFI_INI_COPY_FILES) \
        $(LOCAL_PATH)/wcn.rc:/vendor/etc/init/wcn.rc \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:vendor/etc/permissions/android.hardware.bluetooth_le.xml

PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.wcn.hardware.product=$(SPRD_WCN_HW_MODEL) \
    ro.vendor.wcn.hardware.etcpath=/$(SPRD_WCN_ETC_PATH) \
    ro.bt.bdaddr_path="/data/vendor/bluetooth/btmac.txt" \
    persist.bluetooth.a2dp_offload.cap = "sbc" \
    persist.bluetooth.a2dp_offload.switch = "false" \
    ro.bluetooth.a2dp_offload.supported="true" \
    persist.bluetooth.a2dp_offload.disabled = "false"

PRODUCT_PACKAGES += \
    sprdbt_tty.ko \
    sprd_fm.ko


