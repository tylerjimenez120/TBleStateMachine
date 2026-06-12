/**
 * @file MockBleController.hpp
 * @brief In-memory BLE controller for unit testing.
 *
 * Tracks calls made to the controller interface and exposes test helpers
 * for verifying behavior. Supports failure injection and a manually
 * advanced clock to make tests fast and deterministic.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

#include "IBleController.hpp"

namespace tble::hal {

/**
 * @brief Enumeration of operations that can fail when injected.
 */
enum class BleOperation : uint8_t {
    StartAdvertising,
    StopAdvertising,
    AcceptConnection,
    Disconnect
};

class MockBleController final : public IBleController {
 public:
    MockBleController() = default;

    // ---- IBleController contract ----

    /*
    has_pending_failure_ = is failure injection enabled?
    fail_next_ = if enabled, for which operation?

    Calls startAdvertising()
   ↓
Lock mutex
   ↓
Count attempt (++count)
   ↓
Pending failure for this operation?
   ├─ YES → consume failure, return false
   └─ NO  → set advertising = true, return true
   ↓
Release mutex (automatic on scope exit)
    */
    bool startAdvertising() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++advertising_start_count_;
        //
        if (has_pending_failure_ &&
            fail_next_ == BleOperation::StartAdvertising) {
            has_pending_failure_ = false; // Consume the pending failure — single-use ticket.
            return false;
        }
        radio_is_advertising_ = true;
        return true;
    }

    bool stopAdvertising() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++advertising_stop_count_;
        if (has_pending_failure_ &&
            fail_next_ == BleOperation::StopAdvertising) {
            has_pending_failure_ = false;
            return false;
        }
        radio_is_advertising_ = false;
        return true;
    }

    bool acceptConnection() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++accept_connection_count_;
        if (has_pending_failure_ &&
            fail_next_ == BleOperation::AcceptConnection) {
            has_pending_failure_ = false;
            return false;
        }
        link_is_active_ = true;
        radio_is_advertising_ = false;
        return true;
    }

    bool disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++disconnect_count_;
        if (has_pending_failure_ &&
            fail_next_ == BleOperation::Disconnect) {
            has_pending_failure_ = false;
            return false;
        }
        link_is_active_ = false;
        return true;
    }

    uint64_t millis() const noexcept override {
        return current_millis_.load();
    }

    // ---- Test helpers (not part of the HAL contract) ----

   bool radioIsAdvertising() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return radio_is_advertising_;
    }
 
    /// Whether the link layer has an active connection.
    bool linkIsActive() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return link_is_active_;
    }

    uint32_t advertisingStartCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return advertising_start_count_;
    }

    uint32_t advertisingStopCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return advertising_stop_count_;
    }

    uint32_t acceptConnectionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return accept_connection_count_;
    }

    uint32_t disconnectCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return disconnect_count_;
    }

    /// Force the next call to @p op to return false. // Force the next operation to fail.
    // Simulates hardware errors (I2C NACK, timeouts, etc.) without real hardware.
    void injectFailure(BleOperation op) {
        std::lock_guard<std::mutex> lock(mutex_);
        fail_next_ = op;
        has_pending_failure_ = true;
    }

    /// Advance the simulated clock by @p ms milliseconds. // Advance the simulated clock.
    // Needed to test time-dependent behavior.
    // Lets us test time-dependent logic without waiting in real time.
    // Same idea as the time_scale=0.0 used in TCommand.
    void advanceTime(uint64_t ms) {
        current_millis_.fetch_add(ms);
    }

 private:
    mutable std::mutex mutex_;
    // Hardware radio status (not to be confused with BleState in the
    // state machine layer).
    bool radio_is_advertising_{false}; // Is the radio currently advertising?
    bool link_is_active_{false}; // Is there an active link/connection?
    uint32_t advertising_start_count_{0}; // How many times startAdvertising was attempted
    uint32_t advertising_stop_count_{0}; // How many times stopAdvertising was attempted
    uint32_t accept_connection_count_{0}; // How many times acceptConnection was attempted
    uint32_t disconnect_count_{0}; // How many times disconnect was attempted

    // Holds which operation should fail on the next call.
    BleOperation fail_next_{BleOperation::StartAdvertising}; // Which operation will fail next
    bool has_pending_failure_{false}; // Is there a pending failure?

    std::atomic<uint64_t> current_millis_{0}; // The simulated clock (ms)
    /*
    current_millis_ is atomic because it is read/written outside the mutex.
    Atomic is faster and sufficient for a single independent variable.
    */
};

}  // namespace tble::hal