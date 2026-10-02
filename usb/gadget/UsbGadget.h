#ifndef ANDROID_HARDWARE_USB_GADGET_V1_0_USBGADGET_H
#define ANDROID_HARDWARE_USB_GADGET_V1_0_USBGADGET_H

#include <android/hardware/usb/gadget/1.0/IUsbGadget.h>
#include <hidl/Status.h>
#include <utils/Mutex.h>

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_0 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::sp;

struct UsbGadget : public IUsbGadget {
    UsbGadget();

    // Methods from ::android::hardware::usb::gadget::V1_0::IUsbGadget follow.
    Return<void> setCurrentUsbFunctions(
        uint64_t functions, const sp<IUsbGadgetCallback>& callback,
        uint64_t timeout) override;
    Return<void> getCurrentUsbFunctions(
        const sp<IUsbGadgetCallback>& callback) override;

  private:
    static const char* kGadgetFunctionsPath;
    static const char* kGadgetEnablePath;
    android::Mutex mLock;
    uint64_t currentFunctionsApplied(const std::string& functions);
    std::string functionsToString(uint64_t functions);
    bool writeFunctions(const std::string& functions);
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_USB_GADGET_V1_0_USBGADGET_H
