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

#include <aidl/android/hardware/light/BnLights.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <cstdlib>
#include <cerrno>
#include <mutex>

using namespace aidl::android::hardware::light;
static constexpr auto kBacklight = "/sys/class/backlight/osprey-backlight/brightness";

class Lights final : public BnLights {
    std::mutex lock_;
    int max_;
public:
    explicit Lights(int maximum) : max_(maximum) {}
    ndk::ScopedAStatus getLights(std::vector<HwLight>* lights) override {
        HwLight light;
        light.id = 0;
        light.ordinal = 0;
        light.type = LightType::BACKLIGHT;
        *lights = {light};
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus setLightState(int32_t id, const HwLightState& state) override {
        if (id != 0) return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        const uint32_t color = static_cast<uint32_t>(state.color);
        const int brightness = (77 * ((color >> 16) & 255) +
                                150 * ((color >> 8) & 255) + 29 * (color & 255)) >> 8;
        std::lock_guard guard(lock_);
        if (!android::base::WriteStringToFile(std::to_string(brightness * max_ / 255), kBacklight)) {
            const int error = errno;
            PLOG(ERROR) << "Cannot set panel brightness";
            return ndk::ScopedAStatus::fromServiceSpecificError(error ? error : EIO);
        }
        return ndk::ScopedAStatus::ok();
    }
};

int main() {
    std::string value;
    if (!android::base::ReadFileToString("/sys/class/backlight/osprey-backlight/max_brightness", &value)) {
        PLOG(ERROR) << "Cannot read panel brightness range";
        return 1;
    }
    char* end;
    const long maximum = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || maximum <= 0 || maximum > 65535) return 1;
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    auto service = ndk::SharedRefBase::make<Lights>(static_cast<int>(maximum));
    const std::string name = std::string(Lights::descriptor) + "/default";
    if (AServiceManager_addService(service->asBinder().get(), name.c_str()) != STATUS_OK) return 1;
    ABinderProcess_joinThreadPool();
    return 1;
}
