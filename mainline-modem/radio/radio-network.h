#pragma once
#include "qmi-backend.h"
#include <aidl/android/hardware/radio/RadioError.h>
#include <aidl/android/hardware/radio/network/RegStateResult.h>
#include <aidl/android/hardware/radio/network/SignalStrength.h>
#include <algorithm>
#include <climits>

static QmiBackend qmi;

static aidl::android::hardware::radio::RadioError readSignal(
        aidl::android::hardware::radio::network::SignalStrength& signal) {
    using aidl::android::hardware::radio::RadioError;
    // Unreported measurements must never become zero (a valid strong signal).
    signal.gsm={INT_MAX,INT_MAX,INT_MAX};
    signal.cdma={INT_MAX,INT_MAX};
    signal.evdo={INT_MAX,INT_MAX,INT_MAX};
    signal.lte={INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX};
    signal.tdscdma={INT_MAX,INT_MAX,INT_MAX};
    signal.wcdma={INT_MAX,INT_MAX,INT_MAX,INT_MAX};
    signal.nr={INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX,{},INT_MAX};
    if (!qmi.openPort()) return RadioError::RADIO_NOT_AVAILABLE;
    const auto reply=qmi.nas(0x4f);
    if (!reply.valid || reply.error) return RadioError::MODEM_ERR;
    const auto gsm=reply.field(0x12);
    const auto wcdma=reply.field(0x13);
    const auto lte=reply.field(0x14);
    auto asu=[](int dbm) { return std::clamp((dbm+113)/2,0,31); };
    if (!gsm.empty()) {
        if (gsm.size()!=1 || int8_t(gsm[0])>=0) return RadioError::MODEM_ERR;
        signal.gsm.signalStrength=asu(int8_t(gsm[0]));
    }
    if (!wcdma.empty()) {
        if (wcdma.size()!=3 || int8_t(wcdma[0])>=0) return RadioError::MODEM_ERR;
        signal.wcdma.signalStrength=asu(int8_t(wcdma[0]));
        // QMI reports Ec/Io; do not relabel it as AIDL Ec/No or invent RSCP.
    }
    if (!lte.empty()) {
        if (lte.size()!=6 || int8_t(lte[0])>=0) return RadioError::MODEM_ERR;
        signal.lte.signalStrength=asu(int8_t(lte[0]));
        const int rsrq=-int8_t(lte[1]);
        const int rsrp=-int16_t(QmiReply::u16(lte.data()+2));
        const int snr=int16_t(QmiReply::u16(lte.data()+4));
        if (rsrq>=3 && rsrq<=20) signal.lte.rsrq=rsrq;
        if (rsrp>=44 && rsrp<=140) signal.lte.rsrp=rsrp;
        if (snr>=-200 && snr<=300) signal.lte.rssnr=snr;
    }
    return RadioError::NONE;
}

static aidl::android::hardware::radio::RadioError readRegistration(
        aidl::android::hardware::radio::network::RegStateResult& state) {
    using namespace aidl::android::hardware::radio;
    if (!qmi.openPort()) return RadioError::RADIO_NOT_AVAILABLE;
    const auto reply=qmi.nas(0x24);
    const auto serving=reply.field(1);
    if (!reply.valid || reply.error || serving.size()<5 || serving.size()!=size_t(5+serving[4]) ||
        serving[0]>4 || serving[1]>2 || serving[2]>2 || serving[3]>2) return RadioError::MODEM_ERR;
    // shortcut: registered SIM/cell/PLMN translation remains unsupported until implemented.
    if (serving[0]==1) return RadioError::REQUEST_NOT_SUPPORTED;
    state.regState=static_cast<network::RegState>(serving[0]);
    state.rat=RadioTechnology::UNKNOWN;
    for (size_t i=5; i<serving.size(); ++i) {
        if (serving[i]==4) state.rat=RadioTechnology::GSM;
        if (serving[i]==5) state.rat=RadioTechnology::UMTS;
        if (serving[i]==8) state.rat=RadioTechnology::LTE;
    }
    return RadioError::NONE;
}
