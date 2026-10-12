// Generated from frozen AOSP AIDL V4; edit generate-radio.py and radio-backend.h.
#include "radio-backend.h"
#include "radio-network.h"
#include "radio-sim.h"
#include <csignal>
#include <pthread.h>
using ndk::ScopedAStatus;
namespace radio = aidl::android::hardware::radio;
static radio::RadioResponseInfo info(int32_t serial, radio::RadioError error) {
    return {.type=radio::RadioResponseType::SOLICITED, .serial=serial, .error=error};
}
#include <aidl/android/hardware/radio/config/BnRadioConfig.h>
#include <aidl/android/hardware/radio/config/IRadioConfigResponse.h>
#include <aidl/android/hardware/radio/config/IRadioConfigIndication.h>
class RadioConfig final : public ::aidl::android::hardware::radio::config::BnRadioConfig {
public:
 std::shared_ptr<::aidl::android::hardware::radio::config::IRadioConfigResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::config::IRadioConfigIndication> indication;
 ScopedAStatus getHalDeviceCapabilities(int32_t serial) override {
  if (response) response->getHalDeviceCapabilitiesResponse(info(serial, radio::RadioError::NONE), true);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getNumOfLiveModems(int32_t serial) override {
  if (response) response->getNumOfLiveModemsResponse(info(serial, radio::RadioError::NONE), 1);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getPhoneCapability(int32_t serial) override {

  radio::config::PhoneCapability cap;
  cap.logicalModemIds={0};
  if (response) response->getPhoneCapabilityResponse(info(serial, radio::RadioError::NONE), cap);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSimSlotsStatus(int32_t serial) override {

  radio::sim::CardStatus card;
  const auto error=readCard(card);
  radio::config::SimSlotStatus slot;
  slot.cardState=card.cardState;
  radio::config::SimPortInfo port;
  port.logicalSlotId=0;
  port.portActive=true;
  slot.portInfo={port};
  if (response) response->getSimSlotsStatusResponse(info(serial,error), {slot});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setNumOfLiveModems(int32_t serial, int8_t numOfLiveModems) override {
  if (response) response->setNumOfLiveModemsResponse(info(serial, numOfLiveModems==1 ? radio::RadioError::NONE : radio::RadioError::INVALID_ARGUMENTS));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setPreferredDataModem(int32_t serial, int8_t modemId) override {
  if (response) response->setPreferredDataModemResponse(info(serial, modemId==0 ? radio::RadioError::NONE : radio::RadioError::INVALID_ARGUMENTS));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::config::IRadioConfigResponse>& radioConfigResponse, const std::shared_ptr<::aidl::android::hardware::radio::config::IRadioConfigIndication>& radioConfigIndication) override {
  response=radioConfigResponse; indication=radioConfigIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSimSlotsMapping(int32_t serial, const std::vector<::aidl::android::hardware::radio::config::SlotPortMapping>& slotMap) override {
  if (response) response->setSimSlotsMappingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSimultaneousCallingSupport(int32_t serial) override {
  if (response) response->getSimultaneousCallingSupportResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSimTypeInfo(int32_t serial) override {
  if (response) response->getSimTypeInfoResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSimType(int32_t serial, const std::vector<::aidl::android::hardware::radio::config::SimType>& simTypes) override {
  if (response) response->setSimTypeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/modem/BnRadioModem.h>
#include <aidl/android/hardware/radio/modem/IRadioModemResponse.h>
#include <aidl/android/hardware/radio/modem/IRadioModemIndication.h>
class RadioModem final : public ::aidl::android::hardware::radio::modem::BnRadioModem {
public:
 std::shared_ptr<::aidl::android::hardware::radio::modem::IRadioModemResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::modem::IRadioModemIndication> indication;
 ScopedAStatus enableModem(int32_t serial, bool on) override {
  if (response) response->enableModemResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getBasebandVersion(int32_t serial) override {

  const auto version=backend.revision();
  if (response) response->getBasebandVersionResponse(info(serial, version.empty() ? radio::RadioError::RADIO_NOT_AVAILABLE : radio::RadioError::NONE), version);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getDeviceIdentity(int32_t serial) override {
  if (response) response->getDeviceIdentityResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {}, {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getHardwareConfig(int32_t serial) override {
  if (response) response->getHardwareConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getModemActivityInfo(int32_t serial) override {
  if (response) response->getModemActivityInfoResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getModemStackStatus(int32_t serial) override {

  const bool running=backend.state()!=radio::modem::RadioState::UNAVAILABLE;
  if (response) response->getModemStackStatusResponse(info(serial, radio::RadioError::NONE), running);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getRadioCapability(int32_t serial) override {
  if (response) response->getRadioCapabilityResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus nvReadItem(int32_t serial, ::aidl::android::hardware::radio::modem::NvItem itemId) override {
  if (response) response->nvReadItemResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus nvResetConfig(int32_t serial, ::aidl::android::hardware::radio::modem::ResetNvType resetType) override {
  if (response) response->nvResetConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus nvWriteCdmaPrl(int32_t serial, const std::vector<uint8_t>& prl) override {
  if (response) response->nvWriteCdmaPrlResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus nvWriteItem(int32_t serial, const ::aidl::android::hardware::radio::modem::NvWriteItem& item) override {
  if (response) response->nvWriteItemResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus requestShutdown(int32_t serial) override {
  if (response) response->requestShutdownResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendDeviceState(int32_t serial, ::aidl::android::hardware::radio::modem::DeviceStateType deviceStateType, bool state) override {
  if (response) response->sendDeviceStateResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setRadioCapability(int32_t serial, const ::aidl::android::hardware::radio::modem::RadioCapability& rc) override {
  if (response) response->setRadioCapabilityResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setRadioPower(int32_t serial, bool powerOn, bool forEmergencyCall, bool preferredForEmergencyCall) override {

  const auto result=backend.query(powerOn ? "AT+CFUN=1" : "AT+CFUN=4");
  const bool success=result.find("\nOK\r")!=std::string::npos;
  if (response) response->setRadioPowerResponse(info(serial, success ? radio::RadioError::NONE : radio::RadioError::MODEM_ERR));
  if (success && indication) indication->radioStateChanged(radio::RadioIndicationType::UNSOLICITED, backend.state());
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::modem::IRadioModemResponse>& radioModemResponse, const std::shared_ptr<::aidl::android::hardware::radio::modem::IRadioModemIndication>& radioModemIndication) override {
  response=radioModemResponse; indication=radioModemIndication;
  if (indication) { indication->rilConnected(radio::RadioIndicationType::UNSOLICITED); indication->radioStateChanged(radio::RadioIndicationType::UNSOLICITED, backend.state()); }
  return ScopedAStatus::ok();
 }
 ScopedAStatus getImei(int32_t serial) override {
  if (response) response->getImeiResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/sim/BnRadioSim.h>
#include <aidl/android/hardware/radio/sim/IRadioSimResponse.h>
#include <aidl/android/hardware/radio/sim/IRadioSimIndication.h>
class RadioSim final : public ::aidl::android::hardware::radio::sim::BnRadioSim {
public:
 std::shared_ptr<::aidl::android::hardware::radio::sim::IRadioSimResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::sim::IRadioSimIndication> indication;
 ScopedAStatus areUiccApplicationsEnabled(int32_t serial) override {
  if (response) response->areUiccApplicationsEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus changeIccPin2ForApp(int32_t serial, const std::string& oldPin2, const std::string& newPin2, const std::string& aid) override {
  if (response) response->changeIccPin2ForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus changeIccPinForApp(int32_t serial, const std::string& oldPin, const std::string& newPin, const std::string& aid) override {
  if (response) response->changeIccPinForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus enableUiccApplications(int32_t serial, bool enable) override {
  if (response) response->enableUiccApplicationsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getAllowedCarriers(int32_t serial) override {
  if (response) response->getAllowedCarriersResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCdmaSubscription(int32_t serial) override {
  if (response) response->getCdmaSubscriptionResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {}, {}, {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCdmaSubscriptionSource(int32_t serial) override {
  if (response) response->getCdmaSubscriptionSourceResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getFacilityLockForApp(int32_t serial, const std::string& facility, const std::string& password, int32_t serviceClass, const std::string& appId) override {
  if (response) response->getFacilityLockForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getIccCardStatus(int32_t serial) override {

  radio::sim::CardStatus card;
  const auto error=readCard(card);
  if (response) response->getIccCardStatusResponse(info(serial,error),card);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getImsiForApp(int32_t serial, const std::string& aid) override {
  if (response) response->getImsiForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSimPhonebookCapacity(int32_t serial) override {
  if (response) response->getSimPhonebookCapacityResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSimPhonebookRecords(int32_t serial) override {
  if (response) response->getSimPhonebookRecordsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccCloseLogicalChannel(int32_t serial, int32_t channelId) override {
  if (response) response->iccCloseLogicalChannelResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccIoForApp(int32_t serial, const ::aidl::android::hardware::radio::sim::IccIo& iccIo) override {
  if (response) response->iccIoForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccOpenLogicalChannel(int32_t serial, const std::string& aid, int32_t p2) override {
  if (response) response->iccOpenLogicalChannelResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccTransmitApduBasicChannel(int32_t serial, const ::aidl::android::hardware::radio::sim::SimApdu& message) override {
  if (response) response->iccTransmitApduBasicChannelResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccTransmitApduLogicalChannel(int32_t serial, const ::aidl::android::hardware::radio::sim::SimApdu& message) override {
  if (response) response->iccTransmitApduLogicalChannelResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus reportStkServiceIsRunning(int32_t serial) override {
  if (response) response->reportStkServiceIsRunningResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus requestIccSimAuthentication(int32_t serial, int32_t authContext, const std::string& authData, const std::string& aid) override {
  if (response) response->requestIccSimAuthenticationResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendEnvelope(int32_t serial, const std::string& contents) override {
  if (response) response->sendEnvelopeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendEnvelopeWithStatus(int32_t serial, const std::string& contents) override {
  if (response) response->sendEnvelopeWithStatusResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendTerminalResponseToSim(int32_t serial, const std::string& contents) override {
  if (response) response->sendTerminalResponseToSimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setAllowedCarriers(int32_t serial, const ::aidl::android::hardware::radio::sim::CarrierRestrictions& carriers, ::aidl::android::hardware::radio::sim::SimLockMultiSimPolicy multiSimPolicy) override {
  if (response) response->setAllowedCarriersResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCarrierInfoForImsiEncryption(int32_t serial, const ::aidl::android::hardware::radio::sim::ImsiEncryptionInfo& imsiEncryptionInfo) override {
  if (response) response->setCarrierInfoForImsiEncryptionResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCdmaSubscriptionSource(int32_t serial, ::aidl::android::hardware::radio::sim::CdmaSubscriptionSource cdmaSub) override {
  if (response) response->setCdmaSubscriptionSourceResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setFacilityLockForApp(int32_t serial, const std::string& facility, bool lockState, const std::string& password, int32_t serviceClass, const std::string& appId) override {
  if (response) response->setFacilityLockForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::sim::IRadioSimResponse>& radioSimResponse, const std::shared_ptr<::aidl::android::hardware::radio::sim::IRadioSimIndication>& radioSimIndication) override {
  response=radioSimResponse; indication=radioSimIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSimCardPower(int32_t serial, ::aidl::android::hardware::radio::sim::CardPowerState powerUp) override {
  if (response) response->setSimCardPowerResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setUiccSubscription(int32_t serial, const ::aidl::android::hardware::radio::sim::SelectUiccSub& uiccSub) override {
  if (response) response->setUiccSubscriptionResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplyIccPin2ForApp(int32_t serial, const std::string& pin2, const std::string& aid) override {
  if (response) response->supplyIccPin2ForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplyIccPinForApp(int32_t serial, const std::string& pin, const std::string& aid) override {
  if (response) response->supplyIccPinForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplyIccPuk2ForApp(int32_t serial, const std::string& puk2, const std::string& pin2, const std::string& aid) override {
  if (response) response->supplyIccPuk2ForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplyIccPukForApp(int32_t serial, const std::string& puk, const std::string& pin, const std::string& aid) override {
  if (response) response->supplyIccPukForAppResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplySimDepersonalization(int32_t serial, ::aidl::android::hardware::radio::sim::PersoSubstate persoType, const std::string& controlKey) override {
  if (response) response->supplySimDepersonalizationResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus updateSimPhonebookRecords(int32_t serial, const ::aidl::android::hardware::radio::sim::PhonebookRecordInfo& recordInfo) override {
  if (response) response->updateSimPhonebookRecordsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus iccCloseLogicalChannelWithSessionInfo(int32_t serial, const ::aidl::android::hardware::radio::sim::SessionInfo& sessionInfo) override {
  if (response) response->iccCloseLogicalChannelWithSessionInfoResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/network/BnRadioNetwork.h>
#include <aidl/android/hardware/radio/network/IRadioNetworkResponse.h>
#include <aidl/android/hardware/radio/network/IRadioNetworkIndication.h>
class RadioNetwork final : public ::aidl::android::hardware::radio::network::BnRadioNetwork {
public:
 std::shared_ptr<::aidl::android::hardware::radio::network::IRadioNetworkResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::network::IRadioNetworkIndication> indication;
 ScopedAStatus getAllowedNetworkTypesBitmap(int32_t serial) override {
  if (response) response->getAllowedNetworkTypesBitmapResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getAvailableBandModes(int32_t serial) override {
  if (response) response->getAvailableBandModesResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getAvailableNetworks(int32_t serial) override {
  if (response) response->getAvailableNetworksResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getBarringInfo(int32_t serial) override {
  if (response) response->getBarringInfoResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCdmaRoamingPreference(int32_t serial) override {
  if (response) response->getCdmaRoamingPreferenceResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCellInfoList(int32_t serial) override {
  if (response) response->getCellInfoListResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getDataRegistrationState(int32_t serial) override {

  radio::network::RegStateResult state;
  const auto error=readRegistration(state);
  if (response) response->getDataRegistrationStateResponse(info(serial,error),state);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getImsRegistrationState(int32_t serial) override {
  if (response) response->getImsRegistrationStateResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getNetworkSelectionMode(int32_t serial) override {
  if (response) response->getNetworkSelectionModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getOperator(int32_t serial) override {
  if (response) response->getOperatorResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSignalStrength(int32_t serial) override {

  radio::network::SignalStrength signal;
  const auto error=readSignal(signal);
  if (response) response->getSignalStrengthResponse(info(serial,error),signal);
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSystemSelectionChannels(int32_t serial) override {
  if (response) response->getSystemSelectionChannelsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getVoiceRadioTechnology(int32_t serial) override {
  if (response) response->getVoiceRadioTechnologyResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getVoiceRegistrationState(int32_t serial) override {

  radio::network::RegStateResult state;
  const auto error=readRegistration(state);
  if (response) response->getVoiceRegistrationStateResponse(info(serial,error),state);
  // shortcut: refresh signal with registration polls until NAS indications are handled.
  radio::network::SignalStrength signal;
  if (error==radio::RadioError::NONE && indication && readSignal(signal)==radio::RadioError::NONE)
    indication->currentSignalStrength(radio::RadioIndicationType::UNSOLICITED,signal);
  return ScopedAStatus::ok();
 }
 ScopedAStatus isNrDualConnectivityEnabled(int32_t serial) override {
  if (response) response->isNrDualConnectivityEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus setAllowedNetworkTypesBitmap(int32_t serial, int32_t networkTypeBitmap) override {
  if (response) response->setAllowedNetworkTypesBitmapResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setBandMode(int32_t serial, ::aidl::android::hardware::radio::network::RadioBandMode mode) override {
  if (response) response->setBandModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setBarringPassword(int32_t serial, const std::string& facility, const std::string& oldPassword, const std::string& newPassword) override {
  if (response) response->setBarringPasswordResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCdmaRoamingPreference(int32_t serial, ::aidl::android::hardware::radio::network::CdmaRoamingType type) override {
  if (response) response->setCdmaRoamingPreferenceResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCellInfoListRate(int32_t serial, int32_t rate) override {
  if (response) response->setCellInfoListRateResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setIndicationFilter(int32_t serial, int32_t indicationFilter) override {
  if (response) response->setIndicationFilterResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setLinkCapacityReportingCriteria(int32_t serial, int32_t hysteresisMs, int32_t hysteresisDlKbps, int32_t hysteresisUlKbps, const std::vector<int32_t>& thresholdsDownlinkKbps, const std::vector<int32_t>& thresholdsUplinkKbps, ::aidl::android::hardware::radio::AccessNetwork accessNetwork) override {
  if (response) response->setLinkCapacityReportingCriteriaResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setLocationUpdates(int32_t serial, bool enable) override {
  if (response) response->setLocationUpdatesResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setNetworkSelectionModeAutomatic(int32_t serial) override {
  if (response) response->setNetworkSelectionModeAutomaticResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setNetworkSelectionModeManual(int32_t serial, const std::string& operatorNumeric, ::aidl::android::hardware::radio::AccessNetwork ran) override {
  if (response) response->setNetworkSelectionModeManualResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setNrDualConnectivityState(int32_t serial, ::aidl::android::hardware::radio::network::NrDualConnectivityState nrDualConnectivityState) override {
  if (response) response->setNrDualConnectivityStateResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::network::IRadioNetworkResponse>& radioNetworkResponse, const std::shared_ptr<::aidl::android::hardware::radio::network::IRadioNetworkIndication>& radioNetworkIndication) override {
  response=radioNetworkResponse; indication=radioNetworkIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSignalStrengthReportingCriteria(int32_t serial, const std::vector<::aidl::android::hardware::radio::network::SignalThresholdInfo>& signalThresholdInfos) override {
  if (response) response->setSignalStrengthReportingCriteriaResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSuppServiceNotifications(int32_t serial, bool enable) override {
  if (response) response->setSuppServiceNotificationsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSystemSelectionChannels(int32_t serial, bool specifyChannels, const std::vector<::aidl::android::hardware::radio::network::RadioAccessSpecifier>& specifiers) override {
  if (response) response->setSystemSelectionChannelsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus startNetworkScan(int32_t serial, const ::aidl::android::hardware::radio::network::NetworkScanRequest& request) override {
  if (response) response->startNetworkScanResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus stopNetworkScan(int32_t serial) override {
  if (response) response->stopNetworkScanResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus supplyNetworkDepersonalization(int32_t serial, const std::string& netPin) override {
  if (response) response->supplyNetworkDepersonalizationResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setUsageSetting(int32_t serial, ::aidl::android::hardware::radio::network::UsageSetting usageSetting) override {
  if (response) response->setUsageSettingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getUsageSetting(int32_t serial) override {
  if (response) response->getUsageSettingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setEmergencyMode(int32_t serial, ::aidl::android::hardware::radio::network::EmergencyMode emcModeType) override {
  if (response) response->setEmergencyModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus triggerEmergencyNetworkScan(int32_t serial, const ::aidl::android::hardware::radio::network::EmergencyNetworkScanTrigger& request) override {
  if (response) response->triggerEmergencyNetworkScanResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus cancelEmergencyNetworkScan(int32_t serial, bool resetScan) override {
  if (response) response->cancelEmergencyNetworkScanResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus exitEmergencyMode(int32_t serial) override {
  if (response) response->exitEmergencyModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setNullCipherAndIntegrityEnabled(int32_t serial, bool enabled) override {
  if (response) response->setNullCipherAndIntegrityEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus isNullCipherAndIntegrityEnabled(int32_t serial) override {
  if (response) response->isNullCipherAndIntegrityEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus isN1ModeEnabled(int32_t serial) override {
  if (response) response->isN1ModeEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setN1ModeEnabled(int32_t serial, bool enable) override {
  if (response) response->setN1ModeEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus isCellularIdentifierTransparencyEnabled(int32_t serial) override {
  if (response) response->isCellularIdentifierTransparencyEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCellularIdentifierTransparencyEnabled(int32_t serial, bool enabled) override {
  if (response) response->setCellularIdentifierTransparencyEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSecurityAlgorithmsUpdatedEnabled(int32_t serial, bool enable) override {
  if (response) response->setSecurityAlgorithmsUpdatedEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus isSecurityAlgorithmsUpdatedEnabled(int32_t serial) override {
  if (response) response->isSecurityAlgorithmsUpdatedEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSatellitePlmn(int32_t serial, const std::vector<std::string>& carrierPlmnArray, const std::vector<std::string>& allSatellitePlmnArray) override {
  if (response) response->setSatellitePlmnResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSatelliteEnabledForCarrier(int32_t serial, bool satelliteEnabled) override {
  if (response) response->setSatelliteEnabledForCarrierResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus isSatelliteEnabledForCarrier(int32_t serial) override {
  if (response) response->isSatelliteEnabledForCarrierResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/data/BnRadioData.h>
#include <aidl/android/hardware/radio/data/IRadioDataResponse.h>
#include <aidl/android/hardware/radio/data/IRadioDataIndication.h>
class RadioData final : public ::aidl::android::hardware::radio::data::BnRadioData {
public:
 std::shared_ptr<::aidl::android::hardware::radio::data::IRadioDataResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::data::IRadioDataIndication> indication;
 ScopedAStatus allocatePduSessionId(int32_t serial) override {
  if (response) response->allocatePduSessionIdResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus cancelHandover(int32_t serial, int32_t callId) override {
  if (response) response->cancelHandoverResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus deactivateDataCall(int32_t serial, int32_t cid, ::aidl::android::hardware::radio::data::DataRequestReason reason) override {
  if (response) response->deactivateDataCallResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getDataCallList(int32_t serial) override {
  if (response) response->getDataCallListResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSlicingConfig(int32_t serial) override {
  if (response) response->getSlicingConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus releasePduSessionId(int32_t serial, int32_t id) override {
  if (response) response->releasePduSessionIdResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus setDataAllowed(int32_t serial, bool allow) override {
  if (response) response->setDataAllowedResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setDataProfile(int32_t serial, const std::vector<::aidl::android::hardware::radio::data::DataProfileInfo>& profiles) override {
  if (response) response->setDataProfileResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setDataThrottling(int32_t serial, ::aidl::android::hardware::radio::data::DataThrottlingAction dataThrottlingAction, int64_t completionDurationMillis) override {
  if (response) response->setDataThrottlingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setInitialAttachApn(int32_t serial, const std::optional<::aidl::android::hardware::radio::data::DataProfileInfo>& dataProfileInfo) override {
  if (response) response->setInitialAttachApnResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::data::IRadioDataResponse>& radioDataResponse, const std::shared_ptr<::aidl::android::hardware::radio::data::IRadioDataIndication>& radioDataIndication) override {
  response=radioDataResponse; indication=radioDataIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setupDataCall(int32_t serial, ::aidl::android::hardware::radio::AccessNetwork accessNetwork, const ::aidl::android::hardware::radio::data::DataProfileInfo& dataProfileInfo, bool roamingAllowed, ::aidl::android::hardware::radio::data::DataRequestReason reason, const std::vector<::aidl::android::hardware::radio::data::LinkAddress>& addresses, const std::vector<std::string>& dnses, int32_t pduSessionId, const std::optional<::aidl::android::hardware::radio::data::SliceInfo>& sliceInfo, bool matchAllRuleAllowed) override {
  if (response) response->setupDataCallResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus startHandover(int32_t serial, int32_t callId) override {
  if (response) response->startHandoverResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus startKeepalive(int32_t serial, const ::aidl::android::hardware::radio::data::KeepaliveRequest& keepalive) override {
  if (response) response->startKeepaliveResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus stopKeepalive(int32_t serial, int32_t sessionHandle) override {
  if (response) response->stopKeepaliveResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/messaging/BnRadioMessaging.h>
#include <aidl/android/hardware/radio/messaging/IRadioMessagingResponse.h>
#include <aidl/android/hardware/radio/messaging/IRadioMessagingIndication.h>
class RadioMessaging final : public ::aidl::android::hardware::radio::messaging::BnRadioMessaging {
public:
 std::shared_ptr<::aidl::android::hardware::radio::messaging::IRadioMessagingResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::messaging::IRadioMessagingIndication> indication;
 ScopedAStatus acknowledgeIncomingGsmSmsWithPdu(int32_t serial, bool success, const std::string& ackPdu) override {
  if (response) response->acknowledgeIncomingGsmSmsWithPduResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus acknowledgeLastIncomingCdmaSms(int32_t serial, const ::aidl::android::hardware::radio::messaging::CdmaSmsAck& smsAck) override {
  if (response) response->acknowledgeLastIncomingCdmaSmsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus acknowledgeLastIncomingGsmSms(int32_t serial, bool success, ::aidl::android::hardware::radio::messaging::SmsAcknowledgeFailCause cause) override {
  if (response) response->acknowledgeLastIncomingGsmSmsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus deleteSmsOnRuim(int32_t serial, int32_t index) override {
  if (response) response->deleteSmsOnRuimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus deleteSmsOnSim(int32_t serial, int32_t index) override {
  if (response) response->deleteSmsOnSimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCdmaBroadcastConfig(int32_t serial) override {
  if (response) response->getCdmaBroadcastConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getGsmBroadcastConfig(int32_t serial) override {
  if (response) response->getGsmBroadcastConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getSmscAddress(int32_t serial) override {
  if (response) response->getSmscAddressResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus reportSmsMemoryStatus(int32_t serial, bool available) override {
  if (response) response->reportSmsMemoryStatusResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendCdmaSms(int32_t serial, const ::aidl::android::hardware::radio::messaging::CdmaSmsMessage& sms) override {
  if (response) response->sendCdmaSmsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendCdmaSmsExpectMore(int32_t serial, const ::aidl::android::hardware::radio::messaging::CdmaSmsMessage& sms) override {
  if (response) response->sendCdmaSmsExpectMoreResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendImsSms(int32_t serial, const ::aidl::android::hardware::radio::messaging::ImsSmsMessage& message) override {
  if (response) response->sendImsSmsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendSms(int32_t serial, const ::aidl::android::hardware::radio::messaging::GsmSmsMessage& message) override {
  if (response) response->sendSmsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendSmsExpectMore(int32_t serial, const ::aidl::android::hardware::radio::messaging::GsmSmsMessage& message) override {
  if (response) response->sendSmsExpectMoreResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCdmaBroadcastActivation(int32_t serial, bool activate) override {
  if (response) response->setCdmaBroadcastActivationResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCdmaBroadcastConfig(int32_t serial, const std::vector<::aidl::android::hardware::radio::messaging::CdmaBroadcastSmsConfigInfo>& configInfo) override {
  if (response) response->setCdmaBroadcastConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setGsmBroadcastActivation(int32_t serial, bool activate) override {
  if (response) response->setGsmBroadcastActivationResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setGsmBroadcastConfig(int32_t serial, const std::vector<::aidl::android::hardware::radio::messaging::GsmBroadcastSmsConfigInfo>& configInfo) override {
  if (response) response->setGsmBroadcastConfigResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::messaging::IRadioMessagingResponse>& radioMessagingResponse, const std::shared_ptr<::aidl::android::hardware::radio::messaging::IRadioMessagingIndication>& radioMessagingIndication) override {
  response=radioMessagingResponse; indication=radioMessagingIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setSmscAddress(int32_t serial, const std::string& smsc) override {
  if (response) response->setSmscAddressResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus writeSmsToRuim(int32_t serial, const ::aidl::android::hardware::radio::messaging::CdmaSmsWriteArgs& cdmaSms) override {
  if (response) response->writeSmsToRuimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus writeSmsToSim(int32_t serial, const ::aidl::android::hardware::radio::messaging::SmsWriteArgs& smsWriteArgs) override {
  if (response) response->writeSmsToSimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
};
#include <aidl/android/hardware/radio/voice/BnRadioVoice.h>
#include <aidl/android/hardware/radio/voice/IRadioVoiceResponse.h>
#include <aidl/android/hardware/radio/voice/IRadioVoiceIndication.h>
class RadioVoice final : public ::aidl::android::hardware::radio::voice::BnRadioVoice {
public:
 std::shared_ptr<::aidl::android::hardware::radio::voice::IRadioVoiceResponse> response;
 std::shared_ptr<::aidl::android::hardware::radio::voice::IRadioVoiceIndication> indication;
 ScopedAStatus acceptCall(int32_t serial) override {
  if (response) response->acceptCallResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus cancelPendingUssd(int32_t serial) override {
  if (response) response->cancelPendingUssdResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus conference(int32_t serial) override {
  if (response) response->conferenceResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus dial(int32_t serial, const ::aidl::android::hardware::radio::voice::Dial& dialInfo) override {
  if (response) response->dialResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus emergencyDial(int32_t serial, const ::aidl::android::hardware::radio::voice::Dial& dialInfo, int32_t categories, const std::vector<std::string>& urns, ::aidl::android::hardware::radio::voice::EmergencyCallRouting routing, bool hasKnownUserIntentEmergency, bool isTesting) override {
  if (response) response->emergencyDialResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus exitEmergencyCallbackMode(int32_t serial) override {
  if (response) response->exitEmergencyCallbackModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus explicitCallTransfer(int32_t serial) override {
  if (response) response->explicitCallTransferResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCallForwardStatus(int32_t serial, const ::aidl::android::hardware::radio::voice::CallForwardInfo& callInfo) override {
  if (response) response->getCallForwardStatusResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCallWaiting(int32_t serial, int32_t serviceClass) override {
  if (response) response->getCallWaitingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getClip(int32_t serial) override {
  if (response) response->getClipResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getClir(int32_t serial) override {
  if (response) response->getClirResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {}, {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getCurrentCalls(int32_t serial) override {
  if (response) response->getCurrentCallsResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getLastCallFailCause(int32_t serial) override {
  if (response) response->getLastCallFailCauseResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getMute(int32_t serial) override {
  if (response) response->getMuteResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getPreferredVoicePrivacy(int32_t serial) override {
  if (response) response->getPreferredVoicePrivacyResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus getTtyMode(int32_t serial) override {
  if (response) response->getTtyModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus handleStkCallSetupRequestFromSim(int32_t serial, bool accept) override {
  if (response) response->handleStkCallSetupRequestFromSimResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus hangup(int32_t serial, int32_t gsmIndex) override {
  if (response) response->hangupConnectionResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus hangupForegroundResumeBackground(int32_t serial) override {
  if (response) response->hangupForegroundResumeBackgroundResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus hangupWaitingOrBackground(int32_t serial) override {
  if (response) response->hangupWaitingOrBackgroundResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus isVoNrEnabled(int32_t serial) override {
  if (response) response->isVoNrEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED), {});
  return ScopedAStatus::ok();
 }
 ScopedAStatus rejectCall(int32_t serial) override {
  if (response) response->rejectCallResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus responseAcknowledgement() override {
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendBurstDtmf(int32_t serial, const std::string& dtmf, int32_t on, int32_t off) override {
  if (response) response->sendBurstDtmfResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendCdmaFeatureCode(int32_t serial, const std::string& featureCode) override {
  if (response) response->sendCdmaFeatureCodeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendDtmf(int32_t serial, const std::string& s) override {
  if (response) response->sendDtmfResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus sendUssd(int32_t serial, const std::string& ussd) override {
  if (response) response->sendUssdResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus separateConnection(int32_t serial, int32_t gsmIndex) override {
  if (response) response->separateConnectionResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCallForward(int32_t serial, const ::aidl::android::hardware::radio::voice::CallForwardInfo& callInfo) override {
  if (response) response->setCallForwardResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setCallWaiting(int32_t serial, bool enable, int32_t serviceClass) override {
  if (response) response->setCallWaitingResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setClir(int32_t serial, int32_t status) override {
  if (response) response->setClirResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setMute(int32_t serial, bool enable) override {
  if (response) response->setMuteResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setPreferredVoicePrivacy(int32_t serial, bool enable) override {
  if (response) response->setPreferredVoicePrivacyResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setResponseFunctions(const std::shared_ptr<::aidl::android::hardware::radio::voice::IRadioVoiceResponse>& radioVoiceResponse, const std::shared_ptr<::aidl::android::hardware::radio::voice::IRadioVoiceIndication>& radioVoiceIndication) override {
  response=radioVoiceResponse; indication=radioVoiceIndication;
  return ScopedAStatus::ok();
 }
 ScopedAStatus setTtyMode(int32_t serial, ::aidl::android::hardware::radio::voice::TtyMode mode) override {
  if (response) response->setTtyModeResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus setVoNrEnabled(int32_t serial, bool enable) override {
  if (response) response->setVoNrEnabledResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus startDtmf(int32_t serial, const std::string& s) override {
  if (response) response->startDtmfResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus stopDtmf(int32_t serial) override {
  if (response) response->stopDtmfResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
 ScopedAStatus switchWaitingOrHoldingAndActive(int32_t serial) override {
  if (response) response->switchWaitingOrHoldingAndActiveResponse(info(serial, radio::RadioError::REQUEST_NOT_SUPPORTED));
  return ScopedAStatus::ok();
 }
};
int main() {
 android::base::InitLogging(nullptr, android::base::LogdLogger(android::base::RADIO));
 sigset_t signals;
 sigemptyset(&signals);
 sigaddset(&signals,SIGTERM);
 sigaddset(&signals,SIGINT);
 CHECK_EQ(pthread_sigmask(SIG_BLOCK,&signals,nullptr),0);
 if (!backend.openPort()) return 1;
 ABinderProcess_setThreadPoolMaxThreadCount(1);
 ABinderProcess_startThreadPool();
 auto config=ndk::SharedRefBase::make<RadioConfig>();
 CHECK_EQ(AServiceManager_addService(config->asBinder().get(), (std::string(RadioConfig::descriptor)+"/default").c_str()), STATUS_OK);
 auto modem=ndk::SharedRefBase::make<RadioModem>();
 CHECK_EQ(AServiceManager_addService(modem->asBinder().get(), (std::string(RadioModem::descriptor)+"/slot1").c_str()), STATUS_OK);
 auto sim=ndk::SharedRefBase::make<RadioSim>();
 CHECK_EQ(AServiceManager_addService(sim->asBinder().get(), (std::string(RadioSim::descriptor)+"/slot1").c_str()), STATUS_OK);
 auto network=ndk::SharedRefBase::make<RadioNetwork>();
 CHECK_EQ(AServiceManager_addService(network->asBinder().get(), (std::string(RadioNetwork::descriptor)+"/slot1").c_str()), STATUS_OK);
 auto data=ndk::SharedRefBase::make<RadioData>();
 CHECK_EQ(AServiceManager_addService(data->asBinder().get(), (std::string(RadioData::descriptor)+"/slot1").c_str()), STATUS_OK);
 auto messaging=ndk::SharedRefBase::make<RadioMessaging>();
 CHECK_EQ(AServiceManager_addService(messaging->asBinder().get(), (std::string(RadioMessaging::descriptor)+"/slot1").c_str()), STATUS_OK);
 auto voice=ndk::SharedRefBase::make<RadioVoice>();
 CHECK_EQ(AServiceManager_addService(voice->asBinder().get(), (std::string(RadioVoice::descriptor)+"/slot1").c_str()), STATUS_OK);
 int signal;
 CHECK_EQ(sigwait(&signals,&signal),0);
 qmi.closePort();
 _exit(0);
}
