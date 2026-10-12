// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Sensor.h"
#include <cstdio>
#include <utils/SystemClock.h>
namespace aidl::android::hardware::sensors {
class OspreyLight : public LightSensor {
public:
    OspreyLight(int32_t handle, ISensorsEventCallback* callback) : LightSensor(handle, callback) {
        mSensorInfo.name = "Osprey ambient light";
        mSensorInfo.vendor = "TAOS";
        mSensorInfo.typeAsString = "android.sensor.light";
        mSensorInfo.maxRange = 27000;
        mSensorInfo.resolution = 1;
        mSensorInfo.power = 0.175f;
        mSensorInfo.minDelayUs = 0;
        mSensorInfo.maxDelayUs = 1000000;
        mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE);
        mSamplingPeriodNs = 200000000;
    }
protected:
    std::vector<Event> readEvents() override {
        FILE* file = fopen("/sys/bus/spi/devices/spi0.0/light_lux", "re");
        if (!file) return {};
        int lux;
        int count = fscanf(file, "%d", &lux);
        fclose(file);
        if (count != 1 || lux < 0 || lux > 65535) return {};
        Event event;
        event.sensorHandle = mSensorInfo.sensorHandle;
        event.sensorType = SensorType::LIGHT;
        event.timestamp = ::android::elapsedRealtimeNano();
        event.payload.set<EventPayload::Tag::scalar>(static_cast<float>(lux));
        if (mPreviousEventSet && mPreviousEvent.payload == event.payload) return {};
        mPreviousEvent = event;
        mPreviousEventSet = true;
        return {event};
    }
};
}
