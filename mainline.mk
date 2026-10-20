$(call inherit-product, $(SRC_TARGET_DIR)/product/go_defaults.mk)
PRODUCT_SOONG_NAMESPACES += device/motorola/msm8916-common

PRODUCT_PACKAGES += \
    android.hardware.composer.hwc3-service.drm \
    android.hardware.graphics.allocator-service.minigbm \
    mapper.minigbm \
    gralloc.minigbm_msm \
    audio.primary.mainline \
    android.hardware.audio.service \
    android.hardware.audio@6.0-impl \
    android.hardware.audio.effect@6.0-impl \
    audio.r_submix.default \
    audio.usb.default \
    android.hardware.bluetooth-service.default \
    android.hardware.wifi-service \
    wpa_supplicant \
    hostapd \
    android.hardware.lights-service.osprey \
    android.hardware.health-service.example \
    android.hardware.power-service.example

PRODUCT_COPY_FILES += \
    external/wpa_supplicant_8/wpa_supplicant/aidl/android.hardware.wifi.supplicant-service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/android.hardware.wifi.supplicant-service.rc \
    device/motorola/msm8916-common/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml

PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.audio.primary=mainline \
    debug.mainline.light_probe=true \
    ro.hardware.gralloc=minigbm_msm \
    vendor.hwc.drm.device=/dev/dri/card1 \
    vendor.minigbm.debug=nocompression
PRODUCT_SYSTEM_PROPERTIES += \
    sys.use_memfd=true \
    ro.surface_flinger.default_composition_pixel_format=1

PRODUCT_PACKAGES += libglapi libgallium_dri libEGL_adreno libGLESv1_CM_adreno libGLESv2_adreno
PRODUCT_VENDOR_PROPERTIES += ro.hardware.egl=adreno

PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/mainline-boot/init.osprey.rc:root/init.osprey.rc \
    device/motorola/msm8916-common/mainline-boot/fstab.osprey:system/vendor/etc/fstab.osprey \
    device/motorola/msm8916-common/mainline-boot/osprey-hardware.sh:system/vendor/bin/osprey-hardware.sh \
    device/motorola/msm8916-common/mainline-boot/firmware/a300_pfp.fw:system/vendor/firmware/qcom/a300_pfp.fw \
    device/motorola/msm8916-common/mainline-boot/firmware/a300_pm4.fw:system/vendor/firmware/qcom/a300_pm4.fw

PRODUCT_PACKAGES += ueventd.qcom.rc

# Software-only KeyMint for mainline bring-up; no TEE-backed key protection.
PRODUCT_PACKAGES += android.hardware.security.keymint-service

# Osprey shipped with Android 5.1; retain ARM32 while validating mainline BPF.
PRODUCT_VENDOR_PROPERTIES += ro.board.first_api_level=22 ro.vendor.osprey.mainline_bpf_compat=1

PRODUCT_SOONG_NAMESPACES += hardware/qcom/wlan hardware/qcom/wlan/legacy
PRODUCT_PACKAGES += libcld80211

PRODUCT_PACKAGES += Camera2 android.hardware.camera.provider@2.4-service android.hardware.camera.provider@2.4-impl
PRODUCT_PACKAGES += libcamera_osprey libcamera_osprey_base libcamera-hal ipa_softisp softisp_ipa_proxy osprey_camera_hal_alias osprey_raw64_worker osprey_jpeg64_worker
PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/mainline-camera/media_profiles_V1_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml \
    device/motorola/msm8916-common/mainline-camera/configuration.yaml:$(TARGET_COPY_OUT_VENDOR)/etc/libcamera/configuration.yaml \
    device/motorola/msm8916-common/mainline-camera/camera_hal.yaml:$(TARGET_COPY_OUT_VENDOR)/etc/libcamera/camera_hal.yaml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml

PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/mainline-camera/ipa_softisp.so.sign:$(TARGET_COPY_OUT_VENDOR)/lib/libcamera/ipa/ipa_softisp.so.sign \
    device/motorola/msm8916-common/mainline-camera/s5k3m2.yaml:$(TARGET_COPY_OUT_VENDOR)/share/libcamera/ipa/softisp/s5k3m2.yaml \
    device/motorola/msm8916-common/mainline-camera/uncalibrated.yaml:$(TARGET_COPY_OUT_VENDOR)/share/libcamera/ipa/softisp/uncalibrated.yaml

# MPSS bring-up without SIM; EFS writes stay in the rmtfs RAM shadow.
PRODUCT_PACKAGES += osprey-modem osprey-rmtfs


PRODUCT_COPY_FILES += $(foreach f,$(wildcard device/motorola/msm8916-common/mainline-modem/firmware/*),$(f):$(TARGET_COPY_OUT_VENDOR)/firmware/$(notdir $(f)))

PRODUCT_PACKAGES += android.hardware.radio-service.osprey
PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.telephony.radio.access.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.radio.access.xml

PRODUCT_PACKAGES += android.hardware.sensors-service.osprey

PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml

PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml

PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml



PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml

PRODUCT_PACKAGES += android.hardware.gnss-service.osprey
PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml

# Read mainline thermal zones using the existing configurable Pixel HAL.
PRODUCT_SOONG_NAMESPACES += hardware/google/pixel/thermal
PRODUCT_PACKAGES += android.hardware.thermal-service.pixel
PRODUCT_VENDOR_PROPERTIES += vendor.thermal.config=thermal-osprey.json
PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/configs/thermal-osprey.json:$(TARGET_COPY_OUT_VENDOR)/etc/thermal-osprey.json

# Compile the calculator ahead of time on this low-RAM ARM32 target.
PRODUCT_DEXPREOPT_SPEED_APPS += ExactCalculator

PRODUCT_COPY_FILES += frameworks/av/media/libeffects/data/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml

PRODUCT_VENDOR_PROPERTIES += ro.vendor.camera.req.fmq.size=1048576

PRODUCT_VENDOR_PROPERTIES += ro.vendor.camera.res.fmq.size=1048576

PRODUCT_SYSTEM_PROPERTIES += bluetooth.core.le.vendor_capabilities.enabled=false

# Mainline Venus AVC encoding; keep software codecs as a fallback.
PRODUCT_SOONG_NAMESPACES += external/v4l2_codec2
# Keep the tested Codec2 dependency closure separate from other HAL libraries.
PRODUCT_PACKAGES += android.hardware.media.c2-service-v4l2
# The module and its required policy own native files and checked metadata.
OSPREY_CODEC2_MODULE_FILES := \
    bin/hw/android.hardware.media.c2-service-v4l2 \
    etc/init/android.hardware.media.c2-service-v4l2.rc \
    etc/vintf/manifest/android.hardware.media.c2-service-v4l2.xml \
    etc/seccomp_policy/android.hardware.media.c2-default-seccomp_policy
PRODUCT_COPY_FILES += $(filter-out \
    $(foreach f,$(OSPREY_CODEC2_MODULE_FILES),device/motorola/msm8916-common/mainline-camera/codec2-runtime/$(f):$(TARGET_COPY_OUT_VENDOR)/$(f)),\
    $(call find-copy-subdir-files,*,device/motorola/msm8916-common/mainline-camera/codec2-runtime,$(TARGET_COPY_OUT_VENDOR)))
PRODUCT_VENDOR_PROPERTIES += ro.vendor.v4l2_codec2.encode_concurrent_instances=1
PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/mainline-camera/media_codecs_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_c2.xml \
    device/motorola/msm8916-common/mainline-boot/firmware/qcom/venus-1.8/venus.mbn:$(TARGET_COPY_OUT_VENDOR)/firmware/qcom/venus-1.8/venus.mbn

PRODUCT_SYSTEM_PROPERTIES += media.c2.hal.selection=aidl

PRODUCT_VENDOR_PROPERTIES += ro.vendor.v4l2_codec2.encoder.supported.h264=true

# Keep the Pixel thermal HAL dependency closure in repacked products.
PRODUCT_PACKAGES += pixel-power-ext-V1-ndk pixelatoms-cpp

# PM8916 RTC is read-only; preserve wall time using its battery-backed counter.
PRODUCT_PACKAGES += TimeKeep
