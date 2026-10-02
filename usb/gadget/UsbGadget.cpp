/*
 * Minimal IUsbGadget HAL for legacy android_usb (non-configfs) gadgets,
 * e.g. MSM8916 (osprey). Translates HIDL function sets into
 * /sys/class/android_usb/android0 writes instead of configfs.
 */

#define LOG_TAG "android.hardware.usb.gadget@1.0-service-osprey"

#include "UsbGadget.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include <thread>

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_0 {
namespace implementation {

const char* UsbGadget::kGadgetFunctionsPath =
    "/sys/class/android_usb/android0/functions";
const char* UsbGadget::kGadgetEnablePath =
    "/sys/class/android_usb/android0/enable";

UsbGadget::UsbGadget() {}

std::string UsbGadget::functionsToString(uint64_t functions) {
    std::string out;
    // Keep framework-conventional order: mtp/ptp/midi/rndis first, adb last.
    if (functions & GadgetFunction::MTP) out += "mtp,";
    if (functions & GadgetFunction::PTP) out += "ptp,";
    if (functions & GadgetFunction::MIDI) out += "midi,";
    if (functions & GadgetFunction::RNDIS) out += "rndis,";
    if (functions & GadgetFunction::ACCESSORY) out += "accessory,";
    if (functions & GadgetFunction::AUDIO_SOURCE) out += "audio_source,";
    if (functions & GadgetFunction::ADB) out += "adb,";
    if (!out.empty()) out.pop_back();
    return out;
}

uint64_t UsbGadget::currentFunctionsApplied(const std::string& functions) {
    uint64_t out = 0;
    for (const auto& token : android::base::Split(functions, ",")) {
        std::string f = android::base::Trim(token);
        if (f == "adb") out |= GadgetFunction::ADB;
        else if (f == "mtp") out |= GadgetFunction::MTP;
        else if (f == "ptp") out |= GadgetFunction::PTP;
        else if (f == "midi") out |= GadgetFunction::MIDI;
        else if (f == "rndis") out |= GadgetFunction::RNDIS;
        else if (f == "accessory") out |= GadgetFunction::ACCESSORY;
        else if (f == "audio_source") out |= GadgetFunction::AUDIO_SOURCE;
    }
    return out;
}

bool UsbGadget::writeFunctions(const std::string& functions) {
    // Tear down, apply, bring up. Legacy android_usb needs the bounce.
    if (!android::base::WriteStringToFile("0", kGadgetEnablePath)) {
        LOG(ERROR) << "failed to disable gadget";
        return false;
    }
    if (!android::base::WriteStringToFile(functions, kGadgetFunctionsPath)) {
        LOG(ERROR) << "failed to write functions '" << functions << "'";
        return false;
    }
    if (!android::base::WriteStringToFile("1", kGadgetEnablePath)) {
        LOG(ERROR) << "failed to enable gadget";
        return false;
    }
    return true;
}

Return<void> UsbGadget::setCurrentUsbFunctions(
    uint64_t functions, const sp<IUsbGadgetCallback>& callback,
    uint64_t /*timeout*/) {
    // Acknowledge immediately: sysfs bounce on legacy android_usb can stall
    // past the framework timeout, which makes UsbDeviceManager tear down.
    // Hardware converges asynchronously; getCurrentUsbFunctions always
    // reports the real driver state.
    uint64_t ack = 0;
    Status status = Status::SUCCESS;
    if (functions != 0) {
        std::string str = functionsToString(functions);
        if (str.empty()) {
            status = Status::CONFIGURATION_NOT_SUPPORTED;
        } else {
            ack = functions;
            status = Status::SUCCESS;
        }
    }
    if (callback != nullptr) {
        auto ret = callback->setCurrentUsbFunctionsCb(ack, status);
        if (!ret.isOk()) {
            LOG(ERROR) << "setCurrentUsbFunctionsCb failed";
            return Void();
        }
    }
    if (status != Status::SUCCESS) {
        return Void();
    }
    std::thread([this, functions]() {
        android::Mutex::Autolock _l(mLock);
        std::string current;
        uint64_t currentBits = 0;
        if (android::base::ReadFileToString(kGadgetFunctionsPath, &current)) {
            currentBits = currentFunctionsApplied(android::base::Trim(current));
        }
        std::string enable;
        bool isEnabled = false;
        if (android::base::ReadFileToString(kGadgetEnablePath, &enable)) {
            isEnabled = (android::base::Trim(enable) == "1");
        }
        if (functions == 0) {
            if (isEnabled) {
                android::base::WriteStringToFile("0", kGadgetEnablePath);
            }
            return;
        }
        if (functions == currentBits && isEnabled) {
            return;
        }
        std::string str = functionsToString(functions);
        if (!str.empty()) {
            writeFunctions(str);
        }
    }).detach();
    return Void();
}

Return<void> UsbGadget::getCurrentUsbFunctions(
    const sp<IUsbGadgetCallback>& callback) {
    android::Mutex::Autolock _l(mLock);
    uint64_t functions = 0;
    Status status = Status::ERROR;

    std::string current;
    std::string enable;
    if (android::base::ReadFileToString(kGadgetFunctionsPath, &current)) {
        functions = currentFunctionsApplied(android::base::Trim(current));
        bool isEnabled = false;
        if (android::base::ReadFileToString(kGadgetEnablePath, &enable)) {
            isEnabled = (android::base::Trim(enable) == "1");
        }
        status = isEnabled ? Status::FUNCTIONS_APPLIED : Status::FUNCTIONS_NOT_APPLIED;
    }

    if (callback != nullptr) {
        auto ret = callback->getCurrentUsbFunctionsCb(functions, status);
        if (!ret.isOk()) {
            LOG(ERROR) << "getCurrentUsbFunctionsCb failed";
        }
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
