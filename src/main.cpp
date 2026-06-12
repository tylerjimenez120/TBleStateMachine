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

#include "ble/BleStateMachine.hpp" // la clase orquestadora
#include "hal/MockBleController.hpp" // la hal falsa
//los 5 estados
#include "states/AdvertisingState.hpp"
#include "states/ConnectedState.hpp"
#include "states/ConnectingState.hpp"
#include "states/DisconnectedState.hpp"
#include "states/ErrorState.hpp"

using tble::BleEvent; //crea un evento para enviar
using tble::BleEventType;// el tipo de evento
using tble::BleState; // enum de los estados
using tble::BleStateMachine; //el orquestador 
using tble::hal::MockBleController; // el hal
//los 5 estados
using tble::states::AdvertisingState;
using tble::states::ConnectedState;
using tble::states::ConnectingState;
using tble::states::DisconnectedState;
using tble::states::ErrorState;

namespace {

//función helper para imprimir el estado como string
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

//función helper para imprimir el evento como string
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

//Envía un evento al state machine, espera a que se procese, e imprime el estado resultante.
template <std::size_t N> //template porque BleStateMachine necesita un N (capacidad)
void postAndWait(BleStateMachine<N>& sm, BleEventType ev) { //-> BleStateMachine es template, si lo pasas como parámetro la función también debe ser template.
    std::cout << "  -> pushEvent(" << eventName(ev) << ")\n";
    sm.pushEvent(BleEvent{ev});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::cout << "     state = " << stateName(sm.currentStateId()) << "\n\n";
}
/*
BleStateMachine<N>
   │
   "tiene adentro un..."
   │
   ▼
EventQueue<N>
   │
   "tiene adentro un..."
   │
   ▼
std::array<BleEvent, N>
   │
   "que guarda N de..."
   │
   ▼
BleEvent


BleStateMachine NO es solo un wrapper de EventQueue. Adentro tiene más que eventos — 
tiene también el estado actual, el worker thread, y los flags de control. Por eso ES la máquina en sí.
*/


}  // namespace

int main() {
    std::cout << "=== TBleStateMachine Demo ===\n\n";

    // 1. Build the HAL and the 5 states
    //Los estados que necesitan accionar el hardware llaman a la HAL
    MockBleController hal;
    DisconnectedState disconnected{hal};
    AdvertisingState  advertising{hal};
    ConnectingState   connecting{hal};
    ConnectedState    connected{hal};
    ErrorState        error{hal};

    // 2. Wire up the transitions
    //Cada estado recibe los punteros a los estados a los que puede transicionar, solo se setea para que sepan  donde transicionar
    disconnected.setAdvertising(&advertising);

    advertising.setDisconnected(&disconnected);
    advertising.setConnecting(&connecting);

    connecting.setConnected(&connected);
    connecting.setError(&error);

    connected.setDisconnected(&disconnected);

    error.setDisconnected(&disconnected);
    //Construyes los estados primero. Luego usas los setters para "cablear" las transiciones entre ellos. Soluciona la dependencia circular.

    // 3. Create and start the state machine
    BleStateMachine<16> sm{&disconnected}; //la cola interna tendrá capacidad para 16 eventos -  puntero al estado inicial
    sm.start();
    /*
    Arranca el sistema:

    Llama disconnected.enter() (entrada inicial)
    Lanza el worker thread que va a procesar eventos

    A partir de aquí hay dos threads corriendo en paralelo:

    Main (productor)
    Worker (consumidor)

    1.-start() → entra a Disconnected con enter()
    2.-Lanza el thread → ejecuta run()
    3.-run() saca eventos de la cola y transiciona según corresponda
    */

    std::cout << "Initial state = " << stateName(sm.currentStateId()) << "\n\n"; //Imprime el estado inicial (será "Disconnected").

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
Disconnected   ← vuelve al inicio
    */
    std::cout << "--- Happy path ---\n";
    postAndWait(sm, BleEventType::StartAdvertising);
    postAndWait(sm, BleEventType::ConnectionRequest);
    postAndWait(sm, BleEventType::ConnectionEstablished);
    postAndWait(sm, BleEventType::Disconnect);
    /*
    postAndWait imprime el evento
    Mete el evento en la cola → el worker lo procesa
    Main duerme 30ms
    Main imprime el estado resultante
    */

    // 5. Error path

    /*
    Disconnected
   ↓ StartAdvertising
Advertising
   ↓ ConnectionRequest
Connecting
   ↓ Timeout (handshake falló)
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
    Pone running_ = false
    Llama queue_.shutdown() → despierta al worker dormido
    worker_.join() → espera a que el thread termine
    */
}