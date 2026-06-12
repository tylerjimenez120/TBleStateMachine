/**
 * @file ErrorState.hpp
 * @brief Recovery state after a fatal protocol or hardware error.
 *
 * Transitions:
 *   Reset -> Disconnected
 *
 * All other events are ignored.
 */
#pragma once

#include "../ble/IState.hpp"
#include "../hal/IBleController.hpp"

namespace tble::states {

class ErrorState final : public IState {
 public:
    explicit ErrorState(hal::IBleController& hal) : hal_{hal} {}

    void setDisconnected(IState* s) noexcept { disconnected_ = s; }

    void enter() override {}
    void exit() override {}

    IState* handleEvent(BleEvent event) override {
        if (event.type == BleEventType::Reset) {
            return disconnected_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return BleState::Error; }

 private:
    hal::IBleController& hal_;
    IState* disconnected_{nullptr};
};

}  // namespace tble::states