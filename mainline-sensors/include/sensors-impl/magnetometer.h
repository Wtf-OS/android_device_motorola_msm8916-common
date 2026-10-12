// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Sensor.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <dirent.h>
#include <string>
#include <utils/SystemClock.h>
namespace aidl::android::hardware::sensors {
class OspreyMag : public MagnetometerSensor {
    static std::string devicePath() {
        DIR* dir = opendir("/sys/bus/iio/devices");
        if (!dir) return {};
        std::string result;
        while (auto* entry = readdir(dir)) {
            if (strncmp(entry->d_name, "iio:device", 10)) continue;
            std::string path = std::string("/sys/bus/iio/devices/") + entry->d_name;
            FILE* file = fopen((path + "/name").c_str(), "re");
            if (!file) continue;
            char name[32] = {};
            int count = fscanf(file, "%31s", name);
            fclose(file);
            bool matches = count == 1 && !strcmp(name, "ak09912");
            file = fopen((path + "/../of_node/compatible").c_str(), "re");
            if (file) {
                char compatible[64] = {};
                size_t size = fread(compatible, 1, sizeof(compatible) - 1, file);
                fclose(file);
                matches |= size > 0 && !strcmp(compatible, "asahi-kasei,ak09912");
            }
            if (matches) { result = path; break; }
        }
        closedir(dir);
        return result;
    }
    std::string path;
public:
    static bool available() { return !devicePath().empty(); }
    OspreyMag(int32_t handle, ISensorsEventCallback* callback) : MagnetometerSensor(handle, callback), path(devicePath()) {
        mSensorInfo.name = "Osprey AK09912 magnetic field";
        mSensorInfo.vendor = "Asahi Kasei";
        mSensorInfo.typeAsString = "android.sensor.magnetic_field";
        mSensorInfo.maxRange = 4912;
        mSensorInfo.resolution = 0.15f;
        mSensorInfo.power = 0.1f;
        mSensorInfo.minDelayUs = 100000;
        mSensorInfo.maxDelayUs = 1000000;
        mSensorInfo.flags = 0;
        mSamplingPeriodNs = 100000000;
    }
protected:
    std::vector<Event> readEvents() override {
        float values[3];
        for (int axis = 0; axis < 3; ++axis) {
            std::string prefix = path + "/in_magn_" + char('x' + axis);
            FILE* file = fopen((prefix + "_raw").c_str(), "re");
            if (!file) return {};
            int raw;
            int count = fscanf(file, "%d", &raw);
            fclose(file);
            if (count != 1 || raw < -32768 || raw > 32767) return {};
            file = fopen((prefix + "_scale").c_str(), "re");
            if (!file) return {};
            float scale;
            count = fscanf(file, "%f", &scale);
            fclose(file);
            if (count != 1 || !std::isfinite(scale) || scale <= 0 || scale >= 1) return {};
            values[axis] = raw * scale * 100.0f; // IIO gauss to Android microtesla.
        }
        Event event;
        event.sensorHandle = mSensorInfo.sensorHandle;
        event.sensorType = SensorType::MAGNETIC_FIELD;
        event.timestamp = ::android::elapsedRealtimeNano();
        // Original AKM PAT5 mounting; factory sensitivity is included by IIO scale.
        // shortcut: no hard/soft-iron calibration yet; do not claim heading accuracy.
        EventPayload::Vec3 value = { .x=-values[0], .y=values[1], .z=-values[2],
                                    .status=SensorStatus::UNRELIABLE };
        event.payload.set<EventPayload::Tag::vec3>(value);
        return {event};
    }
};
}
