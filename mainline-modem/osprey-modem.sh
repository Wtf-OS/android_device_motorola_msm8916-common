#!/system/bin/sh
set -eu
ROOT=/data/vendor/osprey-modem
mkdir -p "$ROOT/efs"
chmod 0700 "$ROOT" "$ROOT/efs"
restorecon -RF /vendor/firmware /vendor/bin/osprey-rmtfs "$ROOT"
for mapping in modemst1:19 modemst2:20 fsg:23 fsc:24; do
    name=${mapping%:*}
    number=${mapping#*:}
    grep -qx "PARTNAME=$name" "/sys/class/block/mmcblk0p$number/uevent"
    ln -sf "/dev/block/mmcblk0p$number" "$ROOT/efs/$name"
done
for name in qcom_memshare qcom_q6v5 qcom_q6v5_mss rpmsg_wwan_ctrl; do
    grep -q "^$name " /proc/modules || insmod "/vendor/lib/modules/$name.ko"
done
# Read-only EFS shadow: writes remain in RAM during modem bring-up.
exec /vendor/bin/osprey-rmtfs -r -P -o "$ROOT/efs" -s
