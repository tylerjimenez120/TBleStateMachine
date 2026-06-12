## 🗺️ State machine

![BLE Peripheral State Machine](diagrams/state_machine.png)

# TBleStateMachine — State Design Pattern Lab (BLE Peripheral GAP)

Implementation of the **State Design Pattern** in C++17 modeling the
GAP (Generic Access Profile) state machine of a Bluetooth Low Energy
peripheral. Heap-free, thread-safe, and portable to embedded targets
(ESP32 / Nordic nRF / STM32).

## 🎯 What this lab shows

How to model the connection lifecycle of a BLE peripheral as a state
machine where each state is a class with its own behavior. The pattern
enables:

- **Strict transition rules** — each state knows exactly which events
  it handles; invalid events are ignored
- **Encapsulated logic per state** — adding a new state means adding a
  class, not editing a giant switch
- **Asynchronous event processing** — producers (user, BLE controller,
  timers) push events from any thread; a single worker consumes them
- **Heap-free runtime** — states live as stack objects, the event queue
  is a static circular buffer, no allocation in the hot path

## 📦 What's included

### Hardware abstraction (`src/hal/`)
- `IBleController` — 5-primitive contract (`startAdvertising`,
  `stopAdvertising`, `acceptConnection`, `disconnect`, `millis`)
- `MockBleController` — thread-safe in-memory implementation with
  failure injection and a manually advanced clock

### State machine core (`src/ble/`)
- `BleEvent` — value-type event with `BleEventType` enum (uint8_t)
- `IState` — abstract state contract (`enter` / `exit` / `handleEvent` /
  `id`)
- `EventQueue<N>` — heap-free thread-safe circular FIFO with shutdown
- `BleStateMachine<N>` — orchestrator with worker thread, applies
  `exit → change → enter` on transitions

### Concrete states (`src/states/`)
- `DisconnectedState` — idle; `StartAdvertising` → Advertising
- `AdvertisingState` — broadcasting; `StopAdvertising` / `ConnectionRequest`
- `ConnectingState` — handshake; `ConnectionEstablished` / `Timeout`
- `ConnectedState` — active session; `Disconnect` / `LinkLoss`
- `ErrorState` — recovery; `Reset` → Disconnected

### Demo (`src/main.cpp`)
Runnable demo that wires the full system and walks through both the
happy path (`Disconnected → ... → Connected → Disconnected`) and the
error path (`... → Connecting → Error → Disconnected`).

## 🗺️ State machine

| State | Handles | HAL calls |
|---|---|---|
| Disconnected | StartAdvertising | startAdvertising |
| Advertising | StopAdvertising, ConnectionRequest | stopAdvertising, acceptConnection |
| Connecting | ConnectionEstablished, Timeout | none (passive) |
| Connected | Disconnect, LinkLoss | disconnect (only on Disconnect) |
| Error | Reset | none |

## 🛠️ Build & test

### Prerequisites
- Docker
- Docker Compose v2

### Steps

```bash
docker compose up -d
docker exec -it tble bash

# Inside the container:
mkdir -p build && cd build
cmake ..
make

# Run the test suite (28 tests across 4 suites)
./test/test_suite

# Run the demo
./src/tble_demo
```

## 📁 Layout

```
TBleStateMachine/
├── src/
│   ├── hal/                  # Hardware abstraction
│   │   ├── IBleController.hpp
│   │   └── MockBleController.hpp
│   ├── ble/                  # State machine core (generic)
│   │   ├── BleEvent.hpp
│   │   ├── IState.hpp
│   │   ├── EventQueue.hpp
│   │   └── BleStateMachine.hpp
│   ├── states/               # Concrete states (5)
│   │   ├── DisconnectedState.hpp
│   │   ├── AdvertisingState.hpp
│   │   ├── ConnectingState.hpp
│   │   ├── ConnectedState.hpp
│   │   └── ErrorState.hpp
│   └── main.cpp              # Demo
├── test/                     # GoogleTest suites
└── CMakeLists.txt
```

## 🔌 Porting to a real target

The HAL exposes only **five primitives**. Porting requires implementing
them against the vendor SDK:

| Target | startAdvertising | acceptConnection | disconnect | millis |
|---|---|---|---|---|
| ESP-IDF | `esp_ble_gap_start_advertising` | inherent on connect | `esp_ble_gap_disconnect` | `esp_timer_get_time` |
| Nordic nRF | `sd_ble_gap_adv_start` | inherent on connect event | `sd_ble_gap_disconnect` | `app_timer_cnt_get` |
| Zephyr | `bt_le_adv_start` | inherent on connect event | `bt_conn_disconnect` | `k_uptime_get` |

The rest of the codebase is target-agnostic.

## ⚙️ Threading model

- **One worker thread per state machine** — consumes the event queue
  sequentially, applies transitions
- **Any producer thread** — main loop, ISRs, timers — can push events
  via `pushEvent`; the queue is thread-safe
- **In real embedded** — `std::mutex` + `std::condition_variable` would
  be replaced by FreeRTOS primitives (`xQueueSend` / `xQueueReceive`)

## 💡 Design decisions

### Why store transitions as `IState*` (not concrete types)?
Each state stores pointers to its destination states as `IState*` (the
interface), not as `AdvertisingState*` or `DisconnectedState*`. This
avoids forward declarations and breaks the circular include problem
naturally — every state only depends on the abstract interface.

### Why pass HAL by reference?
All states share a single `IBleController` instance — passed by
reference, not value. The interface is abstract, so copies are
impossible anyway, but the reference also makes the dependency explicit
and testable (inject a mock for tests, a real driver in production).

### Why a template `BleStateMachine<N>`?
The event queue capacity is decided at compile time via the template
parameter. This makes memory usage predictable, eliminates heap
allocation, and lets the compiler optimize the array size — essential
for embedded targets.

### Why a separate worker thread?
Producers (user code, ISRs, timers) must not block waiting for the
state machine to finish processing. A worker thread plus a thread-safe
queue gives a clean producer/consumer model that scales from
single-core MCUs (with RTOS) to multi-threaded application code.

### Why setters for wiring transitions (and not constructor args)?
Each state needs pointers to other states, but the other states
themselves need pointers back — a circular dependency. Constructing
all states first and then wiring them with setters resolves the cycle
without forward declarations or shared ownership tricks.

## 🗺️ Lab roadmap (all merged)

| PR  | Scope |
|-----|-------|
| #1  | Project skeleton + BLE controller HAL |
| #2  | BleEvent + IState + EventQueue |
| #3  | BleStateMachine orchestrator |
| #4  | 5 concrete states + integration tests |
| #5  | Demo (main.cpp) + final README |

## ✅ Tests: 28/28 passing

- **7** MockBleController — radio flags, transitions, failure injection,
  manual clock
- **5** EventQueue — FIFO order, full-rejection, shutdown, circular wrap,
  multi-producer stress
- **6** BleStateMachine — start/stop lifecycle, event delivery,
  transitions, idempotent start
- **10** Concrete states — initial state, all transitions, hardware
  failure handling, irrelevant event handling, error recovery

---

**Developed by:** Jesus Jimenez (Tyler Jmz)
**Systems & Data Engineer | IoT Edge Engineer**
