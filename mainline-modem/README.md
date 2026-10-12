# MPSS bring-up

Validated on osprey without SIM: ARM64 Linux 7 remoteproc boots original MBA/MPSS,
rpmsg_wwan_ctrl exposes DATA5_CNTL as /dev/wwan0qmi0 and DATA1/DATA4 as AT ports.
AT+CGMR reports MPSS.DPM.2.0.2.c1.8-00044-M8936FAAAANUZM-1.
AT+CPIN? reports SIM not inserted. The radio/ directory implements a basic AIDL V4 HAL using the real AT port.

Boot DTB must enable /soc@0/remoteproc@4080000. The current modem-enabled image
is produced by android17-port/modem/build-probe.py. Its kernel/lk2nd are identical
to the validated A16 v24 boot, and its first-stage init stays on System.

Firmware comes from this device's read-only firmware partition. The local extraction and packaging tools unpack mba.mbn.gz and modem.*.gz
into /vendor/firmware. No EFS data is packaged.

rmtfs is statically built for ARM32 using the pinned linux-msm qrtr/rmtfs revisions
in upstream.json. Only stdout buffering is changed for diagnostic logging.
The -r option reads EFS into a RAM shadow and never persists modem writes.
Original modemst1/modemst2/fsg/fsc partitions remain unchanged. Persistence and
calibration writeback must be reviewed before enabling SIM operation.

Android 17 telephony requires newer Radio HAL than the bundled libril HIDL 1.1.
Do not add a fake baseband property or claim telephony is functional.

## AIDL Radio checkpoint

`android.hardware.radio-service.osprey` publishes config, modem, sim, network,
data, messaging and voice V4 interfaces for one backend (slot1). Modem revision
and RF power come from actual AT replies; SIM-absent status comes from QMI UIM.
This firmware's
CGMR reply contains the version line without a trailing OK. Revision is read
before publishing services and kept for this service's lifetime.

Read-only NAS QMI queries through /dev/wwan0qmi0 now provide signal strength and
unregistered/searching/denied/unknown network state. The no-SIM phone reports
WCDMA RSSI around -66 dBm and searching. Missing measurements remain unavailable;
QMI Ec/Io is not relabeled as AIDL Ec/No. Registered-SIM and cell/PLMN translation
still returns REQUEST_NOT_SUPPORTED. QMI clients are released on graceful stop.
Signal is refreshed with voice-registration queries until NAS indications are
implemented. A live framework refresh confirmed WCDMA RSSI -67 dBm in
dumpsys telephony.registry.

UIM Get Card Status (0x2f, service 11) confirms one empty slot, no applications
and all provisioning indices unset. Both IRadioSim and IRadioConfig use that
actual reply. Application decoding with a SIM remains unsupported. Invalid
transport replies/client IDs clear the cached connection for the next query;
the probe verified explicit close/reopen and rejects truncated/duplicate TLVs.
v10 also survived a full normal reboot: Android completed boot, the modem and
HAL were running, and both SIM/slot queries returned absent with zero apps.

Other modem/SIM/network/data/voice/messaging operations return explicit
REQUEST_NOT_SUPPORTED responses. This is not a complete telephony implementation;
SIM insertion, calling, SMS and mobile data are not supported by this checkpoint.
The service currently runs as root in the existing permissive bring-up system.
Move it to a proper radio HAL domain and narrow device-node permissions before
production use. EFS writes remain confined to rmtfs's RAM shadow.

Reproduce with android17-port/modem/generate-radio.py and build-radio.sh in WSL.
prepare-radio.py creates the verified live payload and incremental TWRP ZIP.
