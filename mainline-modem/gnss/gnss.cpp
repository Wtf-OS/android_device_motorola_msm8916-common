#include <aidl/android/hardware/gnss/BnGnss.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>
#include <time.h>
#include "qmi-loc.h"
namespace aidl::android::hardware::gnss {
class Gnss : public BnGnss {
public:
    QmiLoc loc;
    std::shared_ptr<IGnssCallback> callback;
    std::thread worker;
    std::atomic<bool> running{false}, nmea{false}, satellites{false};
    uint32_t interval=1000;
    ~Gnss() { stop(); }
    static ndk::ScopedAStatus unsupported() { return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }
    void report(uint16_t id,const QmiReply& fields) {
        if (!callback) return;
        timespec now{}; clock_gettime(CLOCK_REALTIME,&now);
        const int64_t received=now.tv_sec*1000LL+now.tv_nsec/1000000;
        if (id==0x25 && satellites) {
            const auto list=fields.field(0x10);
            if (list.empty() || list.size()!=1+28u*list[0]) return;
            std::vector<IGnssCallback::GnssSvInfo> svs;
            for (size_t off=1;off<list.size();off+=28) {
                const auto* p=list.data()+off;
                uint32_t valid,system; memcpy(&valid,p,4); memcpy(&system,p+4,4);
                if ((valid&3)!=3) continue;
                IGnssCallback::GnssSvInfo sv{}; sv.svid=QmiReply::u16(p+8);
                switch(system) {
                    case 1: sv.constellation=GnssConstellationType::GPS; break;
                    case 2: sv.constellation=GnssConstellationType::GALILEO; if(sv.svid>300) sv.svid-=300; break;
                    case 3: sv.constellation=GnssConstellationType::SBAS; break;
                    case 4: sv.constellation=GnssConstellationType::BEIDOU; if(sv.svid>200) sv.svid-=200; break;
                    case 5: sv.constellation=GnssConstellationType::GLONASS; if(sv.svid>=65 && sv.svid<=96) sv.svid-=64; break;
                    default: continue;
                }
                if (valid&(1u<<5)) memcpy(&sv.elevationDegrees,p+16,4);
                if (valid&(1u<<6)) memcpy(&sv.azimuthDegrees,p+20,4);
                if (valid&(1u<<7)) memcpy(&sv.cN0Dbhz,p+24,4);
                if (!std::isfinite(sv.cN0Dbhz) || sv.cN0Dbhz<0 || sv.cN0Dbhz>100 ||
                    !std::isfinite(sv.elevationDegrees) || std::abs(sv.elevationDegrees)>90 ||
                    !std::isfinite(sv.azimuthDegrees) || sv.azimuthDegrees<0 || sv.azimuthDegrees>360) continue;
                // The modem exposes antenna C/N0 only; leave baseband C/N0 unknown.
                sv.signalType.emplace();
                sv.signalType->constellation=sv.constellation;
                sv.signalType->codeType="UNKNOWN";
                svs.push_back(sv);
            }
            callback->gnssSvStatusCb(svs);
        }
        if (id==0x26 && nmea) {
            auto bytes=fields.field(1);
            if (!bytes.empty() && bytes.size()<=1024)
                callback->gnssNmeaCb(received,std::string(bytes.begin(),bytes.end()));
        }
        if (id!=0x24) return;
        uint32_t status=~0u;
        if (!QmiLoc::value(fields,1,status) || status!=0) return;
        GnssLocation fix{};
        if (!QmiLoc::value(fields,0x10,fix.latitudeDegrees) || !QmiLoc::value(fields,0x11,fix.longitudeDegrees) ||
            !std::isfinite(fix.latitudeDegrees) || !std::isfinite(fix.longitudeDegrees) ||
            std::abs(fix.latitudeDegrees)>90 || std::abs(fix.longitudeDegrees)>180) return;
        fix.gnssLocationFlags=GnssLocation::HAS_LAT_LONG;
        uint64_t utc=0;
        fix.timestampMillis=QmiLoc::value(fields,0x25,utc) ? static_cast<int64_t>(utc) : received;
        float value;
        if (QmiLoc::value(fields,0x12,value) && std::isfinite(value) && value>0) {
            fix.horizontalAccuracyMeters=value; fix.gnssLocationFlags|=GnssLocation::HAS_HORIZONTAL_ACCURACY;
        }
        if (QmiLoc::value(fields,0x1a,value) && std::isfinite(value)) {
            fix.altitudeMeters=value; fix.gnssLocationFlags|=GnssLocation::HAS_ALTITUDE;
        }
        if (QmiLoc::value(fields,0x18,value) && std::isfinite(value) && value>=0) {
            fix.speedMetersPerSec=value; fix.gnssLocationFlags|=GnssLocation::HAS_SPEED;
        }
        if (QmiLoc::value(fields,0x20,value) && std::isfinite(value) && value>=0 && value<360) {
            fix.bearingDegrees=value; fix.gnssLocationFlags|=GnssLocation::HAS_BEARING;
        }
        clock_gettime(CLOCK_BOOTTIME,&now);
        fix.elapsedRealtime.flags=1;
        fix.elapsedRealtime.timestampNs=now.tv_sec*1000000000LL+now.tv_nsec;
        callback->gnssLocationCb(fix);
    }
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& cb) override {
        if (!cb) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        stop(); callback=cb;
        cb->gnssSetCapabilitiesCb(IGnssCallback::CAPABILITY_SCHEDULING);
        IGnssCallback::GnssSystemInfo info{}; info.yearOfHw=2015; info.name="Qualcomm MSM8916 QMI LOC";
        cb->gnssSetSystemInfoCb(info);
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus start() override {
        if (running) return ndk::ScopedAStatus::ok();
        if (worker.joinable()) stop();
        if (!callback || !loc.start(interval,[this](uint16_t id,const QmiReply& fields){ report(id,fields); }))
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(1,"QMI LOC session unavailable");
        running=true;
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::ENGINE_ON);
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::SESSION_BEGIN);
        worker=std::thread([this]{
            while(running) {
                if (!loc.pollEvents()) {
                    running=false;
                    callback->gnssStatusCb(IGnssCallback::GnssStatusValue::SESSION_END);
                    callback->gnssStatusCb(IGnssCallback::GnssStatusValue::ENGINE_OFF);
                }
            }
        });
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus stop() override {
        const bool wasRunning=running.exchange(false);
        if (worker.joinable()) worker.join();
        loc.stop();
        if (wasRunning && callback) {
            callback->gnssStatusCb(IGnssCallback::GnssStatusValue::SESSION_END);
            callback->gnssStatusCb(IGnssCallback::GnssStatusValue::ENGINE_OFF);
        }
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus close() override { stop(); callback.reset(); return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus setPositionMode(const PositionModeOptions& options) override {
        if (options.minIntervalMs<0) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        const bool restart=running;
        stop(); interval=std::max(1000,options.minIntervalMs);
        return restart ? start() : ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus startNmea() override { nmea=true; return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus stopNmea() override { nmea=false; return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus startSvStatus() override { satellites=true; return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus stopSvStatus() override { satellites=false; return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
                                  int uncertaintyMs) override { return unsupported(); }
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override { return unsupported(); }
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override { return unsupported(); }
    ndk::ScopedAStatus deleteAidingData(GnssAidingData aidingDataFlags) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* iGnssPsds) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssConfiguration(
            std::shared_ptr<IGnssConfiguration>* iGnssConfiguration) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssPowerIndication(
            std::shared_ptr<IGnssPowerIndication>* iGnssPowerIndication) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssMeasurement(
            std::shared_ptr<IGnssMeasurementInterface>* iGnssMeasurement) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssBatching(
            std::shared_ptr<IGnssBatching>* iGnssBatching) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssGeofence(
            std::shared_ptr<IGnssGeofence>* iGnssGeofence) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(
            std::shared_ptr<IGnssNavigationMessageInterface>* iGnssNavigationMessage) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* iAGnss) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* iAGnssRil) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* iGnssDebug) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(
            std::shared_ptr<android::hardware::gnss::visibility_control::IGnssVisibilityControl>*
                    iGnssVisibilityControl) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(
            std::shared_ptr<IGnssAntennaInfo>* iGnssAntennaInfo) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionMeasurementCorrections(
            std::shared_ptr<android::hardware::gnss::measurement_corrections::
                                    IMeasurementCorrectionsInterface>* iMeasurementCorrections) override { return unsupported(); }
    ndk::ScopedAStatus getExtensionGnssAssistanceInterface(
            std::shared_ptr<android::hardware::gnss::gnss_assistance::IGnssAssistanceInterface>*
                    iGnssAssistanceInterface) override { return unsupported(); }
};
}
int main() {
    // One binder thread serializes session changes; only the event reader runs concurrently.
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    auto gnss=ndk::SharedRefBase::make<aidl::android::hardware::gnss::Gnss>();
    if (AServiceManager_addService(gnss->asBinder().get(),"android.hardware.gnss.IGnss/default")!=STATUS_OK) return 1;
    ABinderProcess_joinThreadPool();
    return 1;
}
