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
    IState* current_;//Puntero al estado actual (no posesión)
    std::atomic<bool> running_{false}; //Flag atómico para el loop del worker
    std::thread worker_; //El thread que ejecuta run()
};

}  // namespace tble


/*
Las 3 reglas que rigen este código

El state machine no posee estados — solo guarda un IState* al actual
El thread siempre se cierra limpio — RAII en el destructor + shutdown de la cola
El estado actual decide las transiciones — el state machine solo aplica lo que devuelve handleEvent



Flujo de BleStateMachine


PRODUCTOR                    WORKER THREAD
   pushEvent(e)                 (loop infinito)
        │                              │
        ▼                              ▼
   ┌─────────┐ pop ──► ¿evento?
   │ queue_  │           │
   │ [e][e]  │      no ──┴── sí
   └─────────┘      │        │
        ▲           ▼        ▼
        │        break   current_->handleEvent(e)
        │                    │
        │              devuelve IState* next
        │                    │
        │           ¿next ≠ current y ≠ null?
        │                    │
        │            no ─────┴───── sí
        │            │              │
        │            ▼              ▼
        │         loop         exit() → cambia → enter()
        │                            │
        └────────────────────────────┘
                  vuelve al loop




Las 3 fases del state machine
   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
   │   1. ENTRADA    │   │   2. EVENTOS    │   │   3. SALIDA     │
   ├─────────────────┤   ├─────────────────┤   ├─────────────────┤
   │ start()         │   │ Productores     │   │ stop()          │
   │   ↓             │   │ pushEvent(e)    │   │   ↓             │
   │ current_->      │   │   ↓             │   │ shutdown queue  │
   │ enter()         │   │ Worker thread   │   │   ↓             │
   │   ↓             │   │ procesa eventos │   │ worker termina  │
   │ lanza worker    │   │ y transiciona   │   │   ↓             │
   │                 │   │                 │   │ join()          │
   └─────────────────┘   └─────────────────┘   └─────────────────┘

   
*/