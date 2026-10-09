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
    wpa_supplicant \
    hostapd \
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

PRODUCT_PACKAGES += libglapi libgallium_dri libEGL_adreno libGLESv1_CM_adreno libGLESv2_adreno
PRODUCT_VENDOR_PROPERTIES += ro.hardware.egl=adreno

PRODUCT_COPY_FILES += \
    device/motorola/msm8916-common/mainline-boot/init.osprey.rc:root/init.osprey.rc \
    device/motorola/msm8916-common/mainline-boot/fstab.osprey:system/vendor/etc/fstab.osprey \
    device/motorola/msm8916-common/mainline-boot/osprey-hardware.sh:system/vendor/bin/osprey-hardware.sh \
    device/motorola/msm8916-common/mainline-boot/modules/ak7375.ko:system/vendor/lib/modules/ak7375.ko \
    device/motorola/msm8916-common/mainline-boot/modules/btqca.ko:system/vendor/lib/modules/btqca.ko \
    device/motorola/msm8916-common/mainline-boot/modules/btqcomsmd.ko:system/vendor/lib/modules/btqcomsmd.ko \
    device/motorola/msm8916-common/mainline-boot/modules/cec.ko:system/vendor/lib/modules/cec.ko \
    device/motorola/msm8916-common/mainline-boot/modules/drm_display_helper.ko:system/vendor/lib/modules/drm_display_helper.ko \
    device/motorola/msm8916-common/mainline-boot/modules/drm_exec.ko:system/vendor/lib/modules/drm_exec.ko \
    device/motorola/msm8916-common/mainline-boot/modules/drm_gpuvm.ko:system/vendor/lib/modules/drm_gpuvm.ko \
    device/motorola/msm8916-common/mainline-boot/modules/fan54046-monitor.ko:system/vendor/lib/modules/fan54046-monitor.ko \
    device/motorola/msm8916-common/mainline-boot/modules/fuse.ko:system/vendor/lib/modules/fuse.ko \
    device/motorola/msm8916-common/mainline-boot/modules/gpu-sched.ko:system/vendor/lib/modules/gpu-sched.ko \
    device/motorola/msm8916-common/mainline-boot/modules/i2c-dev.ko:system/vendor/lib/modules/i2c-dev.ko \
    device/motorola/msm8916-common/mainline-boot/modules/i2c-qcom-cci.ko:system/vendor/lib/modules/i2c-qcom-cci.ko \
    device/motorola/msm8916-common/mainline-boot/modules/led-class-flash.ko:system/vendor/lib/modules/led-class-flash.ko \
    device/motorola/msm8916-common/mainline-boot/modules/leds-sky81296.ko:system/vendor/lib/modules/leds-sky81296.ko \
    device/motorola/msm8916-common/mainline-boot/modules/lzo_compress.ko:system/vendor/lib/modules/lzo_compress.ko \
    device/motorola/msm8916-common/mainline-boot/modules/max17042_battery.ko:system/vendor/lib/modules/max17042_battery.ko \
    device/motorola/msm8916-common/mainline-boot/modules/mc.ko:system/vendor/lib/modules/mc.ko \
    device/motorola/msm8916-common/mainline-boot/modules/mdt_loader.ko:system/vendor/lib/modules/mdt_loader.ko \
    device/motorola/msm8916-common/mainline-boot/modules/msm.ko:system/vendor/lib/modules/msm.ko \
    device/motorola/msm8916-common/mainline-boot/modules/osprey-camera-id.ko:system/vendor/lib/modules/osprey-camera-id.ko \
    device/motorola/msm8916-common/mainline-boot/modules/panel-motorola-osprey-inx.ko:system/vendor/lib/modules/panel-motorola-osprey-inx.ko \
    device/motorola/msm8916-common/mainline-boot/modules/qcom-camss.ko:system/vendor/lib/modules/qcom-camss.ko \
    device/motorola/msm8916-common/mainline-boot/modules/qcom_common.ko:system/vendor/lib/modules/qcom_common.ko \
    device/motorola/msm8916-common/mainline-boot/modules/qcom_pil_info.ko:system/vendor/lib/modules/qcom_pil_info.ko \
    device/motorola/msm8916-common/mainline-boot/modules/qcom_wcnss_pil.ko:system/vendor/lib/modules/qcom_wcnss_pil.ko \
    device/motorola/msm8916-common/mainline-boot/modules/rmi_core.ko:system/vendor/lib/modules/rmi_core.ko \
    device/motorola/msm8916-common/mainline-boot/modules/rmi_i2c.ko:system/vendor/lib/modules/rmi_i2c.ko \
    device/motorola/msm8916-common/mainline-boot/modules/s5k3m2.ko:system/vendor/lib/modules/s5k3m2.ko \
    device/motorola/msm8916-common/mainline-boot/modules/s5k5e2.ko:system/vendor/lib/modules/s5k5e2.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-pcm.ko:system/vendor/lib/modules/snd-pcm.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-apq8016-sbc.ko:system/vendor/lib/modules/snd-soc-apq8016-sbc.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-core.ko:system/vendor/lib/modules/snd-soc-core.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-lpass-apq8016.ko:system/vendor/lib/modules/snd-soc-lpass-apq8016.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-lpass-cpu.ko:system/vendor/lib/modules/snd-soc-lpass-cpu.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-lpass-platform.ko:system/vendor/lib/modules/snd-soc-lpass-platform.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-msm8916-analog.ko:system/vendor/lib/modules/snd-soc-msm8916-analog.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-msm8916-digital.ko:system/vendor/lib/modules/snd-soc-msm8916-digital.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-soc-qcom-common.ko:system/vendor/lib/modules/snd-soc-qcom-common.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd-timer.ko:system/vendor/lib/modules/snd-timer.ko \
    device/motorola/msm8916-common/mainline-boot/modules/snd.ko:system/vendor/lib/modules/snd.ko \
    device/motorola/msm8916-common/mainline-boot/modules/ubwc_config.ko:system/vendor/lib/modules/ubwc_config.ko \
    device/motorola/msm8916-common/mainline-boot/modules/v4l2-async.ko:system/vendor/lib/modules/v4l2-async.ko \
    device/motorola/msm8916-common/mainline-boot/modules/v4l2-cci.ko:system/vendor/lib/modules/v4l2-cci.ko \
    device/motorola/msm8916-common/mainline-boot/modules/v4l2-fwnode.ko:system/vendor/lib/modules/v4l2-fwnode.ko \
    device/motorola/msm8916-common/mainline-boot/modules/videobuf2-common.ko:system/vendor/lib/modules/videobuf2-common.ko \
    device/motorola/msm8916-common/mainline-boot/modules/videobuf2-dma-sg.ko:system/vendor/lib/modules/videobuf2-dma-sg.ko \
    device/motorola/msm8916-common/mainline-boot/modules/videobuf2-memops.ko:system/vendor/lib/modules/videobuf2-memops.ko \
    device/motorola/msm8916-common/mainline-boot/modules/videobuf2-v4l2.ko:system/vendor/lib/modules/videobuf2-v4l2.ko \
    device/motorola/msm8916-common/mainline-boot/modules/videodev.ko:system/vendor/lib/modules/videodev.ko \
    device/motorola/msm8916-common/mainline-boot/modules/wcn36xx.ko:system/vendor/lib/modules/wcn36xx.ko \
    device/motorola/msm8916-common/mainline-boot/modules/wcnss_ctrl.ko:system/vendor/lib/modules/wcnss_ctrl.ko \
    device/motorola/msm8916-common/mainline-boot/modules/xt_policy.ko:system/vendor/lib/modules/xt_policy.ko \
    device/motorola/msm8916-common/mainline-boot/modules/xt_quota2.ko:system/vendor/lib/modules/xt_quota2.ko \
    device/motorola/msm8916-common/mainline-boot/modules/xt_u32.ko:system/vendor/lib/modules/xt_u32.ko \
    device/motorola/msm8916-common/mainline-boot/modules/zram.ko:system/vendor/lib/modules/zram.ko \
    device/motorola/msm8916-common/mainline-boot/modules/zsmalloc.ko:system/vendor/lib/modules/zsmalloc.ko \
    device/motorola/msm8916-common/mainline-boot/firmware/a300_pfp.fw:system/vendor/firmware/qcom/a300_pfp.fw \
    device/motorola/msm8916-common/mainline-boot/firmware/a300_pm4.fw:system/vendor/firmware/qcom/a300_pm4.fw

PRODUCT_PACKAGES += ueventd.qcom.rc
