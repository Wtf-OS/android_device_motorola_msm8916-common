#pragma once
#include "qmi-uim.h"
#include <aidl/android/hardware/radio/sim/CardStatus.h>

static aidl::android::hardware::radio::RadioError readCard(
        aidl::android::hardware::radio::sim::CardStatus& card) {
    using namespace aidl::android::hardware::radio;
    card.cardState=sim::CardStatus::STATE_ERROR;
    card.gsmUmtsSubscriptionAppIndex=-1;
    card.cdmaSubscriptionAppIndex=-1;
    card.imsSubscriptionAppIndex=-1;
    card.slotMap.physicalSlotId=0;
    card.slotMap.portId=0;
    const auto reply=qmi.uim(0x2f);
    if (!reply.valid) return RadioError::RADIO_NOT_AVAILABLE;
    if (reply.error) return RadioError::MODEM_ERR;
    const auto value=reply.field(0x10);
    if (value.size()>=15 && value[8]==1 && value[9]==1 && value[14]>0)
        return RadioError::REQUEST_NOT_SUPPORTED;
    const auto decoded=UimCardStatus::parse(value);
    if (!decoded.valid) return RadioError::MODEM_ERR;
    card.cardState=decoded.state;
    card.universalPinState=static_cast<sim::PinState>(decoded.pin);
    return RadioError::NONE;
}
