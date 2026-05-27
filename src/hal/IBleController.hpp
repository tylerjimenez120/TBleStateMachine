/**
 * @file IBleController.hpp
 * @brief Abstract interface to the BLE controller hardware.
 *
 * The HAL exposes only the primitive operations a BLE peripheral can
 * perform: advertising on/off, accept connection, disconnect, and a
 * monotonic time source. State logic (which transitions are valid,
 * how to react to events) lives in the state machine, not here.
 *
 * Implementations must be thread-safe if commands are issued from
 * multiple threads. The MockBleController provided in this lab uses
 * an internal mutex; bare-metal implementations on a single executor
 * thread can skip locking.
 */
#pragma once

#include <cstdint>

namespace tble::hal {

/**
 * @brief Abstract BLE controller interface.
 *
 * Multiple instances may coexist if a device has more than one radio,
 * but each instance owns a hardware resource and is therefore not
 * copyable or movable.
 */
class IBleController {
 public:
    IBleController() = default;
    virtual ~IBleController() = default;

    /*copy and move disable
    Una instancia de HAL representa un recurso de hardware específico. Copiarla no tiene sentido — crearías dos objetos que dicen ser dueños del mismo radio.
    */
    IBleController(const IBleController&) = delete;
    IBleController& operator=(const IBleController&) = delete;
    IBleController(IBleController&&) = delete;
    IBleController& operator=(IBleController&&) = delete;

    /**
     * @brief Start broadcasting advertising packets.
     * @return true on success, false on hardware error.
     */
    virtual bool startAdvertising() = 0;

    /**
     * @brief Stop broadcasting advertising packets.
     * @return true on success, false on hardware error.
     */
    virtual bool stopAdvertising() = 0;

    /**
     * @brief Accept an incoming connection request from a central.
     *
     * Called when the controller signals an inbound connection during
     * advertising. Returns true if the link layer accepted the connection.
     */
    virtual bool acceptConnection() = 0;

    /**
     * @brief Terminate the current connection.
     * @return true on success, false if no connection was active.
     */
    virtual bool disconnect() = 0;

    /**
     * @brief Monotonic millisecond counter since boot.
     *
     * Used by the state machine to detect timeouts (e.g. connecting
     * attempt that takes too long).
     */
    virtual uint64_t millis() const noexcept = 0;
};

}  // namespace tble::hal