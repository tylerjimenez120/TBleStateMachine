/**
 * @file main.cpp
 * @brief Demo of the BLE peripheral state machine.
 *
 * Wires up the full system (HAL + state machine + 5 concrete states)
 * and walks through a realistic event sequence, printing the state
 * after each transition.
 */
#include <chrono>
#include <iostream>
#include <thread>

#include "ble/BleStateMachine.hpp" // the orchestrator class
#include "hal/MockBleController.hpp" // the mock HAL
// the 5 states
#include "states/AdvertisingState.hpp"
#include "states/ConnectedState.hpp"
#include "states/ConnectingState.hpp"
#include "states/DisconnectedState.hpp"
#include "states/ErrorState.hpp"

using tble::BleEvent; // create an event to send
using tble::BleEventType; // event type
using tble::BleState; // enum of the states
using tble::BleStateMachine; // the orchestrator
using tble::hal::MockBleController; // the HAL
// the 5 states
using tble::states::AdvertisingState;
using tble::states::ConnectedState;
using tble::states::ConnectingState;
using tble::states::DisconnectedState;
using tble::states::ErrorState;

namespace {

// Helper function to print the state as a string
const char* stateName(BleState s) {
    switch (s) {
        case BleState::Disconnected: return "Disconnected";
        case BleState::Advertising:  return "Advertising";
        case BleState::Connecting:   return "Connecting";
        case BleState::Connected:    return "Connected";
        case BleState::Error:        return "Error";
    }
    return "Unknown";
}

// Helper function to print the event as a string
const char* eventName(BleEventType t) {
    switch (t) {
        case BleEventType::StartAdvertising:      return "StartAdvertising";
        case BleEventType::StopAdvertising:       return "StopAdvertising";
        case BleEventType::ConnectionRequest:     return "ConnectionRequest";
        case BleEventType::ConnectionEstablished: return "ConnectionEstablished";
        case BleEventType::Disconnect:            return "Disconnect";
        case BleEventType::LinkLoss:              return "LinkLoss";
        case BleEventType::Timeout:               return "Timeout";
        case BleEventType::Reset:                 return "Reset";
    }
    return "Unknown";
}

// Sends an event to the state machine, waits for it to be processed,
// and prints the resulting state.
template <std::size_t N> // template because BleStateMachine needs an N (capacity)
void postAndWait(BleStateMachine<N>& sm, BleEventType ev) { // -> BleStateMachine is a template; if passed as a parameter, the function must also be a template.
    std::cout << "  -> pushEvent(" << eventName(ev) << ")\n";
    sm.pushEvent(BleEvent{ev});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::cout << "     state = " << stateName(sm.currentStateId()) << "\n\n";
}
/*
BleStateMachine<N>
   │
   "contains inside a..."
   │
   ▼
EventQueue<N>
   │
   "contains inside a..."
   │
   ▼
std::array<BleEvent, N>
   │
   "which holds N of..."
   │
   ▼
BleEvent


BleStateMachine is NOT just a wrapper around EventQueue. Inside it has more than events —
it also has the current state, the worker thread, and the control flags.
That is why it IS the machine itself.
*/


}  // namespace

int main() {
    std::cout << "=== TBleStateMachine Demo ===\n\n";

    // 1. Build the HAL and the 5 states
    // States that need to drive the hardware call the HAL.
    MockBleController hal;
    DisconnectedState disconnected{hal};
    AdvertisingState  advertising{hal};
    ConnectingState   connecting{hal};
    ConnectedState    connected{hal};
    ErrorState        error{hal};

    // 2. Wire up the transitions
    // Each state receives pointers to the states it can transition to;
    // setters are used so each state knows where to go next.
    disconnected.setAdvertising(&advertising);

    advertising.setDisconnected(&disconnected);
    advertising.setConnecting(&connecting);

    connecting.setConnected(&connected);
    connecting.setError(&error);

    connected.setDisconnected(&disconnected);

    error.setDisconnected(&disconnected);
    // Build the states first. Then use the setters to "wire" the
    // transitions between them. This resolves the circular dependency.

    // 3. Create and start the state machine
    BleStateMachine<16> sm{&disconnected}; // internal queue capacity = 16 events - pointer to the initial state
    sm.start();
    /*
    Boots the system:

    Calls disconnected.enter() (initial entry)
    Launches the worker thread that will process events

    From here on, two threads run in parallel:

    Main (producer)
    Worker (consumer)

    1.- start() → enters Disconnected via enter()
    2.- Launches the thread → executes run()
    3.- run() pops events from the queue and transitions accordingly
    */

    std::cout << "Initial state = " << stateName(sm.currentStateId()) << "\n\n"; // Prints the initial state (will be "Disconnected").

    // 4. Happy path
    /*
Disconnected
   ↓ StartAdvertising
Advertising
   ↓ ConnectionRequest
Connecting
   ↓ ConnectionEstablished
Connected
   ↓ Disconnect
Disconnected   ← back to the start
    */
    std::cout << "--- Happy path ---\n";
    postAndWait(sm, BleEventType::StartAdvertising);
    postAndWait(sm, BleEventType::ConnectionRequest);
    postAndWait(sm, BleEventType::ConnectionEstablished);
    postAndWait(sm, BleEventType::Disconnect);
    /*
    postAndWait prints the event
    Pushes the event into the queue → the worker processes it
    Main sleeps 30ms
    Main prints the resulting state
    */

    // 5. Error path

    /*
    Disconnected
   ↓ StartAdvertising
Advertising
   ↓ ConnectionRequest
Connecting
   ↓ Timeout (handshake failed)
Error
   ↓ Reset
Disconnected   ← recovery
    */
    std::cout << "--- Error path ---\n";
    postAndWait(sm, BleEventType::StartAdvertising);
    postAndWait(sm, BleEventType::ConnectionRequest);
    postAndWait(sm, BleEventType::Timeout);
    postAndWait(sm, BleEventType::Reset);

    // 6. Shutdown cleanly
    sm.stop();
    std::cout << "=== Demo finished ===\n";
    return 0;
    /*
    Sets running_ = false
    Calls queue_.shutdown() → wakes up the sleeping worker
    worker_.join() → waits for the thread to finish
    */
}