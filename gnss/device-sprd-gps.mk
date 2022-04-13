ifeq ($(strip $(SUPPORT_GNSS_HARDWARE)), true)

$(warning shell echo "device-sprd-gps_mk_entry")
PRODUCT_COPY_FILES += \
	vendor/sprd/modules/wcn/vendor/gnss/misc/spirentroot.cer:/vendor/etc/spirentroot.cer \
	vendor/sprd/modules/wcn/vendor/gnss/misc/supl.xml:/vendor/etc/supl.xml \
	vendor/sprd/modules/wcn/vendor/gnss/misc/config.xml:/vendor/etc/config.xml
$(warning shell echo "device-sprd-gps_mk_entry1")
PRODUCT_PACKAGES += \
    gpsd \
    liblte

$(warning shell echo "device-sprd-gps_mk_end")
endif


