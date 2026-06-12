/**
 * @file AdvertisingState.hpp
 * @brief BLE peripheral broadcasting advertising packets.
 *
 * Transitions:
 *   StopAdvertising    -> Disconnected (after successful HAL call)
 *   ConnectionRequest  -> Connecting   (after successful HAL accept)
 *
 * All other events are ignored.
 */
#pragma once

#include "../ble/IState.hpp"
#include "../hal/IBleController.hpp"

namespace tble::states {

class AdvertisingState final : public IState {
 public:
    explicit AdvertisingState(hal::IBleController& hal) : hal_{hal} {}

    void setDisconnected(IState* s) noexcept { disconnected_ = s; }
    void setConnecting(IState* s) noexcept { connecting_ = s; }

    void enter() override {}
    void exit() override {}

    IState* handleEvent(BleEvent event) override {
        if (event.type == BleEventType::StopAdvertising) {
            if (!hal_.stopAdvertising()) {
                return nullptr;
            }
            return disconnected_;
        }
        if (event.type == BleEventType::ConnectionRequest) {
            if (!hal_.acceptConnection()) {
                return nullptr;
            }
            return connecting_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return BleState::Advertising; }

 private:
    hal::IBleController& hal_;
    IState* disconnected_{nullptr};
    IState* connecting_{nullptr};
};

}  // namespace tble::states