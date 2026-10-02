#ifndef ANDROID_HARDWARE_USB_V1_0_USBOSP_H
#define ANDROID_HARDWARE_USB_V1_0_USBOSP_H

#include <android/hardware/usb/1.0/IUsb.h>
#include <hidl/Status.h>
#include <utils/Mutex.h>

namespace android {
namespace hardware {
namespace usb {
namespace V1_0 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::sp;

struct Usb : public IUsb {
    Usb();

    Return<void> switchRole(const hidl_string& portName,
                            const PortRole& role) override;
    Return<void> setCallback(const sp<IUsbCallback>& callback) override;
    Return<void> queryPortStatus() override;

  private:
    android::Mutex mLock;
    sp<IUsbCallback> mCallback;
    void queryPortStatusHelper(hidl_vec<PortStatus>* status);
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace usb
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_USB_V1_0_USBOSP_H
