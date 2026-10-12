// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Sensor.h"
#include <cstdio>
#include <utils/SystemClock.h>
namespace aidl::android::hardware::sensors {
class OspreyProximity : public ProximitySensor {
public:
    OspreyProximity(int32_t handle, ISensorsEventCallback* callback) : ProximitySensor(handle, callback) {
        mSensorInfo.name = "Osprey proximity";
        mSensorInfo.vendor = "TAOS";
        mSensorInfo.typeAsString = "android.sensor.proximity";
        mSensorInfo.maxRange = 100;
        mSensorInfo.resolution = 1;
        mSensorInfo.power = 3.0f;
        mSensorInfo.minDelayUs = 0;
        mSensorInfo.maxDelayUs = 1000000;
        // GPIO52 wakes the kernel; AOSP FMQ acknowledges wake-up events.
        mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE | SensorInfo::SENSOR_FLAG_BITS_WAKE_UP);
        mSamplingPeriodNs = 200000000;
    }
protected:
    std::vector<Event> readEvents() override {
        FILE* file = fopen("/sys/bus/spi/devices/spi0.0/proximity", "re");
        if (!file) return {};
        int lux;
        int count = fscanf(file, "%d", &lux);
        fclose(file);
        if (count != 1 || lux < 0 || lux > 2) return {};
        Event event;
        event.sensorHandle = mSensorInfo.sensorHandle;
        event.sensorType = SensorType::PROXIMITY;
        event.timestamp = ::android::elapsedRealtimeNano();
        event.payload.set<EventPayload::Tag::scalar>(lux == 0 ? 100.0f : lux == 1 ? 3.0f : 1.0f);
        if (mPreviousEventSet && mPreviousEvent.payload == event.payload) return {};
        mPreviousEvent = event;
        mPreviousEventSet = true;
        return {event};
    }
};
}
