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
    android.hardware.light@2.0-service.msm8916 \
    android.hardware.health-service.example \
    android.hardware.power-service.example

PRODUCT_COPY_FILES += \
    external/wpa_supplicant_8/wpa_supplicant/aidl/vendor/android.hardware.wifi.supplicant-service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/android.hardware.wifi.supplicant-service.rc \
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
    debug.mainline.light_probe=true \
    ro.hardware.gralloc=minigbm_msm \
    vendor.hwc.drm.device=/dev/dri/card1 \
    vendor.minigbm.debug=nocompression
PRODUCT_SYSTEM_PROPERTIES += \
    sys.use_memfd=true \
    debug.mainline.allow_memfd=true \
    ro.surface_flinger.default_composition_pixel_format=5

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
PRODUCT_PACKAGES += libcamera_osprey libcamera_osprey_base libcamera-hal ipa_softisp softisp_ipa_proxy osprey_camera_hal_alias
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
