/**
 * @file ConnectedState.hpp
 * @brief BLE peripheral with an active connection.
 *
 * Transitions:
 *   Disconnect -> Disconnected (after successful HAL call)
 *   LinkLoss   -> Disconnected (no HAL call; link is already gone)
 *
 * All other events are ignored.
 */
#pragma once

#include "../ble/IState.hpp"
#include "../hal/IBleController.hpp"

namespace tble::states {

class ConnectedState final : public IState {
 public:
    explicit ConnectedState(hal::IBleController& hal) : hal_{hal} {}

    void setDisconnected(IState* s) noexcept { disconnected_ = s; }

    void enter() override {}
    void exit() override {}

    IState* handleEvent(BleEvent event) override {
        if (event.type == BleEventType::Disconnect) {
            if (!hal_.disconnect()) {
                return nullptr;
            }
            return disconnected_;
        }
        if (event.type == BleEventType::LinkLoss) {
            // Hardware lost the link by itself; no HAL call needed.
            return disconnected_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return BleState::Connected; }

 private:
    hal::IBleController& hal_;
    IState* disconnected_{nullptr};
};

}  // namespace tble::states