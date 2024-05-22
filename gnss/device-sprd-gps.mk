ifeq ($(strip $(SUPPORT_GNSS_HARDWARE)), true)

$(warning shell echo "device-sprd-gps_mk_entry")
PRODUCT_COPY_FILES += \
	vendor/sprd/modules/wcn/vendor/gnss/misc/suplrootca.pem:/vendor/etc/suplrootca.pem \
	vendor/sprd/modules/wcn/vendor/gnss/misc/client.crt:/vendor/etc/client.crt \
	vendor/sprd/modules/wcn/vendor/gnss/misc/client_key.pem:/vendor/etc/client_key.pem \
	vendor/sprd/modules/wcn/vendor/gnss/misc/supl.xml:/vendor/etc/supl.xml \
	vendor/sprd/modules/wcn/vendor/gnss/misc/config.xml:/vendor/etc/config.xml

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,vendor/sprd/modules/wcn/vendor/gnss/misc/supl,$(TARGET_COPY_OUT_VENDOR)/etc/supl)
$(warning shell echo "device-sprd-gps_mk_entry1")

ifneq (,$(filter $(strip $(SPRD_MODULES_NAVC_PATH)),lite lite-Integ))
GNSSSOC := marlin3lite
else
ifneq (,$(filter $(strip $(SPRD_MODULES_NAVC_PATH)),ge2))
GNSSSOC := ge2
else
GNSSSOC := marlin3
endif
endif
$(warning shell echo "device-sprd-gps_mk gnss chip "$(GNSSSOC))
$(call add_soong_config_namespace, navcore)
$(call add_soong_config_var_value, navcore, gnsschip, $(GNSSSOC))


PRODUCT_PACKAGES += \
    gpsd \
    libnavcore

$(warning shell echo "device-sprd-gps_mk_end")
endif


