#!/system/bin/sh
set -eu
load() {
    name="$1"
    shift
    if ! grep -q "^${name//-/_} " /proc/modules; then
        insmod "/vendor/lib/modules/$name.ko" "$@"
    fi
}
load fuse
mkdir -p /config /dev/usb-ffs/adb
grep -q " /config " /proc/mounts || mount -t configfs configfs /config
G=/config/usb_gadget/g1
mkdir -p "$G/strings/0x409" "$G/configs/b.1/strings/0x409" "$G/functions/ffs.adb"
echo 0x18d1 > "$G/idVendor"
echo 0x4ee7 > "$G/idProduct"
echo Motorola > "$G/strings/0x409/manufacturer"
getprop ro.serialno > "$G/strings/0x409/serialnumber"
echo 'Moto G3 Android mainline' > "$G/strings/0x409/product"
echo 250 > "$G/configs/b.1/MaxPower"
# Framework opens ep0 and writes descriptors before binding the gadget.
for function in mtp ptp; do
    mkdir -p "$G/functions/ffs.$function" "/dev/usb-ffs/$function"
    mount -t functionfs -o uid=1000,gid=1024,rmode=0770,fmode=0660 "$function" "/dev/usb-ffs/$function"
done
mount -t functionfs -o uid=2000,gid=2000,rmode=0770,fmode=0660 adb /dev/usb-ffs/adb
for controller in /sys/class/udc/*; do
    [ -e "$controller" ] || continue
    setprop sys.usb.controller "${controller##*/}"
    break
done
# Firmware partition is read-only; never change the bootloader's firmware.
mount -t ext4 -o ro,noload /dev/block/mmcblk0p1 /firmware
echo /vendor/firmware > /sys/module/firmware_class/parameters/path
for name in qcom_common mdt_loader qcom_pil_info wcnss_ctrl wcn36xx qcom_wcnss_pil; do load "$name"; done
load interconnect_qcom
load icc-smd-rpm
load qnoc-msm8916
load bd65b60
for name in drm_exec drm_gpuvm gpu-sched ubwc_config cec drm_display_helper panel-motorola-osprey-inx; do load "$name"; done
if [ -e /sys/bus/platform/drivers/simple-framebuffer/83200000.framebuffer ]; then
    echo 83200000.framebuffer > /sys/bus/platform/drivers/simple-framebuffer/unbind
fi
load msm separate_gpu_kms=1
for name in rmi_core rmi_i2c max17042_battery snd snd-timer snd-pcm snd-compress snd-soc-core qcom_pdr_msg pdr_interface apr snd-q6dsp-common q6core q6afe q6afe-clocks q6afe-dai q6adm q6routing q6asm q6asm-dai snd-soc-qcom-common snd-soc-msm8916-analog snd-soc-msm8916-digital snd-soc-apq8016-sbc; do load "$name"; done
load fan54046-monitor control_charging=1
for name in af_key btqca btqcomsmd xt_policy xt_u32 xt_quota2 lzo_compress zsmalloc zram; do load "$name"; done
for name in i2c-qcom-cci mc videodev v4l2-cci v4l2-async v4l2-fwnode videobuf2-common videobuf2-memops videobuf2-dma-sg videobuf2-v4l2 qcom-camss ak7375 s5k3m2 s5k5e2 i2c-dev led-class-flash leds-sky81296; do load "$name"; done
echo 536870912 > /sys/block/zram0/disksize
mkswap /dev/block/zram0
swapon /dev/block/zram0
if [ -e /sys/class/backlight/osprey-backlight/brightness ]; then
    chown system:system /sys/class/backlight/osprey-backlight/brightness
    chmod 0660 /sys/class/backlight/osprey-backlight/brightness
fi
echo 'OSPREY A16: native hardware initialization complete'

# Optional during bring-up: keep the system usable if sensorhub probing fails.
if ! grep -q '^stml0xx_accel ' /proc/modules; then
    insmod /vendor/lib/modules/stml0xx-accel.ko || echo "sensorhub module failed" >&2
fi

load ak8975
