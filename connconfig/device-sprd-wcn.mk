#
# Spreadtrum WCN configuration
#

# Check envionment variables
ifeq ($(TARGET_BOARD),)
$(error WCN connectivity.mk should include after TARGET_BOARD)
endif

ifeq ($(BOARD_HAVE_SPRD_WCN_COMBO),)
$(error PRODUCT MK should have BOARD_HAVE_SPRD_WCN_COMBO config)
endif

# Config
# Help fix the board conflict without TARGET_BOARD
ifeq ($(SPRD_WCN_HW_CONFIG),)
SPRD_WCN_HW_CONFIG := $(TARGET_BOARD)
endif

# Legacy
# Help compatible with legacy project
ifeq ($(PLATFORM_VERSION),6.0)
SPRD_WCN_ETC_PATH := system/etc
endif

ifeq ($(PLATFORM_VERSION),4.4.4)
SPRD_WCN_ETC_PATH := system/etc
endif

SPRD_WCNBT_CHISET := $(BOARD_HAVE_SPRD_WCN_COMBO)
SPRD_WCN_HW_MODEL := $(BOARD_HAVE_SPRD_WCN_COMBO)
UNISOC_WCN_KERNEL_PATH := $(KERNEL_PATH)

# add wifi regulatory file to vendor/firmware
WIFI_REGULATORY_FILES := \
    regulatory.db \
    regulatory.db.p7s
VENDOR_FIRMWARE_PATH := vendor/firmware
GENERATE_REGULATORY_COPY_FILES += $(foreach own, $(WIFI_REGULATORY_FILES), \
    $(if $(wildcard $(LOCAL_PATH)/regulatory/$(own)), \
        $(LOCAL_PATH)/regulatory/$(own):$(VENDOR_FIRMWARE_PATH)/$(own)))
PRODUCT_COPY_FILES += \
    $(GENERATE_REGULATORY_COPY_FILES)

ifeq ($(wildcard $(LOCAL_PATH)/$(SPRD_WCN_HW_MODEL)/$(SPRD_WCN_HW_CONFIG)),)
$(error wcn chip ini configuration miss. please fix it, and don't take a random one)
# If you don't know how to choose, please contact project PM!
#
# steven.chen
endif

$(call add_soong_config_namespace,libbt)
$(call add_soong_config_var_value,libbt,btsoc,$(BOARD_HAVE_SPRD_WCN_COMBO))
$(call inherit-product, vendor/sprd/modules/wcn/vendor/connconfig/$(SPRD_WCN_HW_MODEL)/connectivity.mk)

PRODUCT_PACKAGES += \
    libbqbbt \
    libbt-vendor \
    libwcn-vendor \
    btools \
    android.hardware.bluetooth@1.1-service.unisoc \
    libbluetooth_audio_session_unisoc \
    libbluetooth_audio_session_aidl_unisoc \
    vendor.sprd.hardware.bluetooth.audio-impl\
    audio.bluetooth.default \
    libbt-sprd_suite \
    libbt-sprd_eut \
    libbt-ssp_bt \
    libfm-sprd_eut \
    autotestfm \
    fm_tools \
    btaudio_offload_if
