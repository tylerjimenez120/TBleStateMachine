# TBleStateMachine — BLE Peripheral State Machine Lab

A C++17 implementation of the **State Design Pattern** modeling the GAP
(Generic Access Profile) state machine of a Bluetooth Low Energy peripheral.

> 🚧 **Work in progress** — shipped incrementally as stacked PRs.
> See the [Roadmap](#roadmap) below.

## 🎯 Objective

Demonstrate the State pattern in a realistic embedded context: the connection
lifecycle of a BLE peripheral (advertising → connecting → connected →
disconnected, with error and timeout handling).

## 📦 Current scope (PR #1)

- Project skeleton with CMake build system
- Dockerized development environment
- Hardware abstraction (`IBleController`) with mock implementation
- Strict compile flags, clang-format and clang-tidy policies
- GoogleTest via CMake `FetchContent`

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
./test/test_suite
```

## 🗺️ Roadmap

| PR  | Status | Scope |
|-----|--------|-------|
| #1  | 🚧     | Project skeleton + BLE controller HAL |
| #2  | ⏳     | IState + BleEvent + EventQueue        |
| #3  | ⏳     | BleStateMachine + transitions table   |
| #4  | ⏳     | Concrete states (Disconnected, Advertising, Connecting, Connected, Error) |
| #5  | ⏳     | Demo + final README                   |

---

**Developed by:** Jesus Jimenez (Tyler Jmz)