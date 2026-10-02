#define LOG_TAG "android.hardware.usb.gadget@1.0-service-osprey"

#include <android-base/logging.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>

#include "UsbGadget.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::usb::gadget::V1_0::IUsbGadget;
using android::hardware::usb::gadget::V1_0::implementation::UsbGadget;
using android::status_t;
using android::OK;

int main() {
    configureRpcThreadpool(4, true /*callerWillJoin*/);

    android::sp<IUsbGadget> service = new UsbGadget();
    status_t status = service->registerAsService("default");
    if (status != OK) {
        LOG(ERROR) << "Cannot register USB Gadget HAL service: " << status;
        return 1;
    }

    LOG(INFO) << "USB Gadget HAL (legacy android_usb) ready";
    joinRpcThreadpool();
    return 1;  // should never get here
}
