/**
 * @file BleEvent.hpp
 * @brief Events that drive the BLE peripheral state machine.
 *
 * An event represents something that happened (user action, BLE controller
 * notification, timer expiration) and is consumed by the state machine to
 * decide whether to transition.
 *
 * Events are plain value types — cheap to copy, no heap, no virtual dispatch.
 * They flow through an EventQueue (PR #2) into the state machine.
 */
#pragma once

#include <cstdint>

namespace tble {

/**
 * @brief Type tag identifying which event occurred.
 */
enum class BleEventType : uint8_t {
    StartAdvertising,       ///< Request to start broadcasting (user-initiated)
    StopAdvertising,        ///< Request to stop broadcasting (user-initiated)
    ConnectionRequest,      ///< Incoming connection from a central (HW-initiated)
    ConnectionEstablished,  ///< Handshake completed successfully (HW-initiated)
    Disconnect,             ///< Request to drop the connection (user or peer)
    LinkLoss,               ///< Link unexpectedly lost (HW-initiated)
    Timeout,                ///< Timer expired (internal)
    Reset                   ///< Recover from Error state
};

/**
 * @brief Event passed to the state machine.
 *
 * Currently a thin wrapper around the type tag. Future evolutions could
 * add per-type payloads (e.g. central address on ConnectionRequest) via
 * a union or std::variant — but a plain enum-tagged struct is sufficient
 * for this lab.
 */
struct BleEvent {
    BleEventType type{BleEventType::Reset};

    BleEvent() noexcept = default;
    explicit BleEvent(BleEventType t) noexcept : type{t} {}
};

}  // namespace tble