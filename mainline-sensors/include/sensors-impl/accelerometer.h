// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Sensor.h"
#include <cstdio>
#include <utils/SystemClock.h>

namespace aidl::android::hardware::sensors {
class OspreyAccel : public AccelSensor {
public:
    OspreyAccel(int32_t handle, ISensorsEventCallback* callback) : AccelSensor(handle, callback) {
        mSensorInfo.name = "Osprey sensorhub accelerometer";
        mSensorInfo.vendor = "Motorola";
        mSensorInfo.typeAsString = "android.sensor.accelerometer";
        mSensorInfo.resolution = 0.00980665f;
        mSensorInfo.power = 0.5f;
        mSensorInfo.minDelayUs = 40000;
        mSensorInfo.maxDelayUs = 1000000;
        mSensorInfo.flags = 0;
        mSamplingPeriodNs = 40000000;
    }
protected:
    std::vector<Event> readEvents() override {
        FILE* file = fopen("/sys/bus/spi/devices/spi0.0/accel_xyz", "re");
        if (!file) return {};
        int x, y, z;
        int count = fscanf(file, "%d %d %d", &x, &y, &z);
        fclose(file);
        if (count != 3 || x < -32768 || x > 32767 || y < -32768 || y > 32767 ||
            z < -32768 || z > 32767) return {};
        Event event;
        event.sensorHandle = mSensorInfo.sensorHandle;
        event.sensorType = SensorType::ACCELEROMETER;
        event.timestamp = ::android::elapsedRealtimeNano();
        // Firmware applies the original Osprey orientation (5,5) and accel swap.
        EventPayload::Vec3 value = {
            .x = x * 0.00980665f, .y = y * 0.00980665f, .z = z * 0.00980665f,
            .status = SensorStatus::ACCURACY_MEDIUM,
        };
        event.payload.set<EventPayload::Tag::vec3>(value);
        return {event};
    }
};
}
