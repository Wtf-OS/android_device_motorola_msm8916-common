/*
 * Minimal IUsb HAL for legacy micro-USB (non-PD, non-Type-C) ports,
 * e.g. MSM8916 (osprey). Reports a single fixed-function UFP port and
 * answers queries synchronously so UsbPortManager never times out.
 */

#define LOG_TAG "android.hardware.usb@1.0-service-osprey"

#include "Usb.h"

#include <android-base/logging.h>

namespace android {
namespace hardware {
namespace usb {
namespace V1_0 {
namespace implementation {

Usb::Usb() : mCallback(nullptr) {}

void Usb::queryPortStatusHelper(hidl_vec<PortStatus>* status) {
    PortStatus currentPort = {};
    currentPort.portName = "port0";
    // Legacy micro-USB on this device: device-only unless OTG adapter.
    currentPort.currentMode = PortMode::UFP;
    currentPort.canChangeMode = false;
    currentPort.canChangeDataRole = false;
    currentPort.canChangePowerRole = false;
    currentPort.currentDataRole = PortDataRole::DEVICE;
    currentPort.currentPowerRole = PortPowerRole::SINK;
    currentPort.supportedModes = PortMode::UFP;
    status->resize(1);
    (*status)[0] = currentPort;
}

Return<void> Usb::switchRole(const hidl_string& portName,
                             const PortRole& role) {
    // No PD/Type-C role switching on legacy hardware; acknowledge so the
    // framework does not wait for a callback that will never come.
    android::Mutex::Autolock _l(mLock);
    if (mCallback != nullptr) {
        auto ret = mCallback->notifyRoleSwitchStatus(portName, role,
                                                     Status::SUCCESS);
        if (!ret.isOk()) {
            LOG(ERROR) << "notifyRoleSwitchStatus failed";
        }
    } else {
        LOG(INFO) << "switchRole ignored (no callback registered)";
    }
    return Void();
}

Return<void> Usb::setCallback(const sp<IUsbCallback>& callback) {
    android::Mutex::Autolock _l(mLock);
    mCallback = callback;
    LOG(INFO) << "USB port status callback registered";
    return Void();
}

Return<void> Usb::queryPortStatus() {
    android::Mutex::Autolock _l(mLock);
    hidl_vec<PortStatus> status;
    queryPortStatusHelper(&status);
    if (mCallback != nullptr) {
        auto ret = mCallback->notifyPortStatusChange(status, Status::SUCCESS);
        if (!ret.isOk()) {
            LOG(ERROR) << "notifyPortStatusChange failed";
        }
    } else {
        LOG(INFO) << "queryPortStatus: no callback registered, "
                  << status.size() << " port(s) present";
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace usb
}  // namespace hardware
}  // namespace android
