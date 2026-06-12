/**
 * @file BleStateMachine.hpp
 * @brief Owns the current state and orchestrates transitions.
 *
 * The state machine holds all concrete states as members (no heap), owns
 * the event queue, and runs a single tick loop that pops events and asks
 * the current state to handle them. Transitions follow the standard
 * exit(old) -> change pointer -> enter(new) sequence.
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "BleEvent.hpp"
#include "EventQueue.hpp"
#include "IState.hpp"

namespace tble {

/**
 * @brief BLE state machine driver.
 *
 * @tparam QueueCapacity Maximum events queued waiting for processing.
 *
 * The state machine does not OWN the concrete states — those live in the
 * derived/composing object (see PR #4). It only knows the IState interface
 * and the EventQueue.
 *
 * Lifecycle:
 *   - construct with initial state
 *   - start() spins the worker thread
 *   - producers push events via postEvent()
 *   - stop() shuts down queue and joins the worker
 */
template <std::size_t QueueCapacity>
class BleStateMachine {
 public:
    explicit BleStateMachine(IState* initial)
        : current_{initial} {}

    BleStateMachine(const BleStateMachine&) = delete;
    BleStateMachine& operator=(const BleStateMachine&) = delete;
    BleStateMachine(BleStateMachine&&) = delete;
    BleStateMachine& operator=(BleStateMachine&&) = delete;

    ~BleStateMachine() { stop(); }

    /// Start the worker thread. Calls enter() on the initial state first.
    void start() {
        if (running_.exchange(true)) return;
        current_->enter();
        worker_ = std::thread([this] { run(); });
    }

    /// Signal stop, wake the worker, and join.
    void stop() {
        if (!running_.exchange(false)) return;
        queue_.shutdown();
        if (worker_.joinable()) worker_.join();
    }

/// Producer side: enqueue an event for the state machine to process.
bool pushEvent(BleEvent event) {
    return queue_.tryPush(event);
}

/// Identifier of the current state (for logging and tests).
BleState currentStateId() const noexcept {
        return current_->id();
    }

 private:
    void run() {
        while (running_.load()) {
            auto event = queue_.pop();
            if (!event) break;  // queue shut down

            IState* next = current_->handleEvent(*event);

            if (next != nullptr && next != current_) {
                current_->exit();
                current_ = next;
                current_->enter();
            }
        }
    }

    EventQueue<QueueCapacity> queue_;
    IState* current_; // Pointer to the current state (non-owning)
    std::atomic<bool> running_{false}; // Atomic flag for the worker loop
    std::thread worker_; // The thread that executes run()
};

}  // namespace tble


/*
The 3 rules that govern this code:

The state machine does not own the states — it only holds an IState* to the current one.
The thread always shuts down cleanly — RAII in the destructor + queue shutdown.
The current state decides transitions — the state machine just applies what handleEvent returns.



BleStateMachine flow


PRODUCER                     WORKER THREAD
   pushEvent(e)                 (infinite loop)
        │                              │
        ▼                              ▼
   ┌─────────┐ pop ──► event?
   │ queue_  │           │
   │ [e][e]  │      no ──┴── yes
   └─────────┘      │        │
        ▲           ▼        ▼
        │        break   current_->handleEvent(e)
        │                    │
        │              returns IState* next
        │                    │
        │           next != current and != null?
        │                    │
        │            no ─────┴───── yes
        │            │              │
        │            ▼              ▼
        │         loop         exit() → change → enter()
        │                            │
        └────────────────────────────┘
                  back to the loop




The 3 phases of the state machine
   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
   │   1. ENTRY      │   │   2. EVENTS     │   │   3. EXIT       │
   ├─────────────────┤   ├─────────────────┤   ├─────────────────┤
   │ start()         │   │ Producers       │   │ stop()          │
   │   ↓             │   │ pushEvent(e)    │   │   ↓             │
   │ current_->      │   │   ↓             │   │ shutdown queue  │
   │ enter()         │   │ Worker thread   │   │   ↓             │
   │   ↓             │   │ processes events│   │ worker ends     │
   │ launches worker │   │ and transitions │   │   ↓             │
   │                 │   │                 │   │ join()          │
   └─────────────────┘   └─────────────────┘   └─────────────────┘

   
*/