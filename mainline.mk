$(call inherit-product, $(SRC_TARGET_DIR)/product/go_defaults.mk)
PRODUCT_SOONG_NAMESPACES += device/motorola/msm8916-common

PRODUCT_PACKAGES += \
    android.hardware.composer.hwc3-service.drm \
    android.hardware.graphics.allocator-service.minigbm \
    mapper.minigbm \
    gralloc.minigbm_msm \
    audio.primary.mainline \
    android.hardware.audio@2.0-service \
    android.hardware.audio@2.0-impl \
    android.hardware.audio.effect@2.0-impl \
    audio.r_submix.default \
    audio.usb.default \
    android.hardware.bluetooth-service.default \
    android.hardware.wifi-service \
    android.hardware.wifi.supplicant-service \
    android.hardware.wifi.hostapd-service \
    android.hardware.light@2.0-service.msm8916 \
    android.hardware.health-service.example \
    android.hardware.power-service.example

PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml

PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.audio=mainline \
    ro.hardware.gralloc=minigbm_msm \
    vendor.hwc.drm.device=/dev/dri/card1 \
    vendor.minigbm.debug=nocompression
PRODUCT_SYSTEM_PROPERTIES += \
    sys.use_memfd=true \
    debug.mainline.allow_memfd=true \
    ro.surface_flinger.default_composition_pixel_format=5
