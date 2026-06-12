/**
 * @file DisconnectedState.hpp
 * @brief BLE peripheral in idle state — not advertising, no connection.
 *
 * Transitions:
 *   StartAdvertising -> Advertising (after successful HAL call)
 *
 * All other events are ignored.
 */
#pragma once

#include "../ble/IState.hpp"
#include "../hal/IBleController.hpp"

namespace tble::states {

class DisconnectedState final : public IState {
 public:
    explicit DisconnectedState(hal::IBleController& hal) : hal_{hal} {}

    void setAdvertising(IState* s) noexcept { advertising_ = s; }

    void enter() override {}
    void exit() override {}

    IState* handleEvent(BleEvent event) override {
        if (event.type == BleEventType::StartAdvertising) {
            if (!hal_.startAdvertising()) {
                return nullptr;
            }
            return advertising_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return BleState::Disconnected; }

 private:
    hal::IBleController& hal_;
    IState* advertising_{nullptr};
};

}  // namespace tble::states