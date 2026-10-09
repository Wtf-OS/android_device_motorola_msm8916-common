/*
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.light@2.0-service.msm8916"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>

#include "Light.h"

// libhwbinder:
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

// Generated HIDL files
using android::hardware::light::V2_0::ILight;
using android::hardware::light::V2_0::implementation::Light;

const static std::string kLcdBacklightPath = "/sys/class/leds/lcd-backlight/brightness";
const static std::string kLcdMaxBacklightPath = "/sys/class/leds/lcd-backlight/max_brightness";
const static std::string kChargingLedPath = "/sys/class/leds/charging/brightness";

// No light devices are exposed by the simpledrm RAM probe.
class ProbeLight final : public ILight {
    android::hardware::Return<android::hardware::light::V2_0::Status> setLight(
            android::hardware::light::V2_0::Type,
            const android::hardware::light::V2_0::LightState&) override {
        return android::hardware::light::V2_0::Status::LIGHT_NOT_SUPPORTED;
    }
    android::hardware::Return<void> getSupportedTypes(getSupportedTypes_cb callback) override {
        callback({});
        return android::hardware::Void();
    }
};

int main() {
    if (android::base::GetBoolProperty("debug.mainline.light_probe", false)) {
        configureRpcThreadpool(1, true);
        std::ofstream backlight("/sys/class/backlight/osprey-backlight/brightness");
        android::sp<ILight> service;
        if (backlight) {
            service = new Light({std::move(backlight), 255}, std::ofstream("/dev/null"));
            LOG(INFO) << "Mainline native panel backlight ready.";
        } else {
            service = new ProbeLight();
        }
        if (service->registerAsService() != android::OK) return 1;
        LOG(INFO) << "Mainline Light HAL ready.";
        joinRpcThreadpool();
        return 1;
    }

    uint32_t lcdMaxBrightness = 255;

    std::ofstream lcdBacklight(kLcdBacklightPath);
    if (!lcdBacklight) {
        LOG(ERROR) << "Failed to open " << kLcdBacklightPath << ", error=" << errno
                   << " (" << strerror(errno) << ")";
        return -errno;
    }

    std::ifstream lcdMaxBacklight(kLcdMaxBacklightPath);
    if (!lcdMaxBacklight) {
        LOG(ERROR) << "Failed to open " << kLcdMaxBacklightPath << ", error=" << errno
                   << " (" << strerror(errno) << ")";
        return -errno;
    } else {
        lcdMaxBacklight >> lcdMaxBrightness;
    }

    std::ofstream chargingLed(kChargingLedPath);
    if (!chargingLed) {
        LOG(ERROR) << "Failed to open " << kChargingLedPath << ", error=" << errno
                   << " (" << strerror(errno) << ")";
        return -errno;
    }

    android::sp<ILight> service = new Light(
            {std::move(lcdBacklight), lcdMaxBrightness},
            std::move(chargingLed));

    configureRpcThreadpool(1, true);

    android::status_t status = service->registerAsService();

    if (status != android::OK) {
        LOG(ERROR) << "Cannot register Light HAL service";
        return 1;
    }

    LOG(INFO) << "Light HAL Ready.";
    joinRpcThreadpool();
    // Under normal cases, execution will not reach this line.
    LOG(ERROR) << "Light HAL failed to join thread pool.";
    return 1;
}
