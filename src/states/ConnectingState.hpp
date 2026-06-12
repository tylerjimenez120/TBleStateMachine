/**
 * @file ConnectingState.hpp
 * @brief BLE peripheral in connection handshake.
 *
 * Transitions:
 *   ConnectionEstablished -> Connected
 *   Timeout               -> Error
 *
 * All other events are ignored.
 */
#pragma once

#include "../ble/IState.hpp"
#include "../hal/IBleController.hpp"

namespace tble::states {

class ConnectingState final : public IState {
 public:
    explicit ConnectingState(hal::IBleController& hal) : hal_{hal} {}

    void setConnected(IState* s) noexcept { connected_ = s; }
    void setError(IState* s) noexcept { error_ = s; }

    void enter() override {}
    void exit() override {}

    IState* handleEvent(BleEvent event) override {
        if (event.type == BleEventType::ConnectionEstablished) {
            return connected_;
        }
        if (event.type == BleEventType::Timeout) {
            return error_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return BleState::Connecting; }

 private:
    hal::IBleController& hal_;
    IState* connected_{nullptr};
    IState* error_{nullptr};
};

}  // namespace tble::states