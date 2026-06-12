/**
 * @file IState.hpp
 * @brief Abstract base class for BLE state machine states.
 *
 * The State pattern represents each state as a concrete class. The state
 * machine holds a pointer to the current state and delegates event handling
 * to it. The state itself decides whether (and to which state) to transition.
 *
 * Lifecycle:
 *   - enter()  is called when the state machine enters this state
 *   - handleEvent(e) is called for every incoming event
 *   - exit()   is called just before the state machine leaves this state
 *
 * If handleEvent() returns nullptr, the state machine stays in the current
 * state (event ignored). If it returns a different IState*, the machine
 * transitions: exit() the old, enter() the new.
 */
#pragma once

#include <cstdint>

#include "BleEvent.hpp"

namespace tble {

/**
 * @brief Identifier for the current state — useful for logging and tests.
 */
enum class BleState : uint8_t {
    Disconnected,
    Advertising,
    Connecting,
    Connected,
    Error
};

/**
 * @brief Abstract state interface.
 *
 * Concrete states (DisconnectedState, AdvertisingState, etc.) implement
 * this contract. They hold a reference to the BLE HAL and use it to drive
 * hardware operations during enter/exit/handleEvent.
 */
class IState {
 public:
    IState() = default;
    virtual ~IState() = default;

    // Copy/move disabled - States are unique objects (internal singletons of
    // the state machine). They are only referenced via pointer.
    IState(const IState&) = delete;
    IState& operator=(const IState&) = delete;
    IState(IState&&) = delete;
    IState& operator=(IState&&) = delete;

    /**
     * @brief Called once when the state machine transitions into this state.
     *
     * Typical use: start advertising, start a timer, log entry.
     */
    virtual void enter() = 0;

    /**
     * @brief Called once when the state machine transitions out of this state.
     *
     * Typical use: stop advertising, cancel a timer, log exit.
     */
    virtual void exit() = 0;

    /**
     * @brief Decide what to do with an incoming event.
     *
     * @return nullptr to stay in the current state (event ignored),
     *         or a pointer to the next state to transition into.
     *
     * The state machine owns the returned pointer (in concrete implementations
     * it points to long-lived singleton state instances — no heap allocation
     * per transition).
     */
    virtual IState* handleEvent(BleEvent event) = 0;

    /**
     * @brief Identifier of this state (for logging, tests, observability).
     */
    virtual BleState id() const noexcept = 0;
};

}  // namespace tble