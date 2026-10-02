#define LOG_TAG "android.hardware.usb@1.0-service-osprey"

#include <android-base/logging.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>

#include "Usb.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::usb::V1_0::IUsb;
using android::hardware::usb::V1_0::implementation::Usb;
using android::status_t;
using android::OK;

int main() {
    configureRpcThreadpool(4, true /*callerWillJoin*/);

    android::sp<IUsb> service = new Usb();
    status_t status = service->registerAsService("default");
    if (status != OK) {
        LOG(ERROR) << "Cannot register USB HAL service: " << status;
        return 1;
    }

    LOG(INFO) << "USB HAL (legacy micro-USB) ready";
    joinRpcThreadpool();
    return 1;  // should never get here
}
