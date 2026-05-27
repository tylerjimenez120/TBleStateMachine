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
    has_pending_failure_ = ¿está activada la inyección de fallo?
    fail_next_ = si está activada, ¿para qué operación?

    Llaman startAdvertising()
   ↓
Lockea mutex
   ↓
Cuenta intento (++count)
   ↓
¿Fallo pendiente para esta operación?
   ├─ SÍ → consume fallo, return false
   └─ NO → marca advertising = true, return true
   ↓
Libera mutex (automático al salir)
    */
    bool startAdvertising() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++advertising_start_count_;
        //
        if (has_pending_failure_ &&
            fail_next_ == BleOperation::StartAdvertising) {
            has_pending_failure_ = false; //Consume el fallo pendiente — se gasta como un "ticket de un solo uso".
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

    /// Force the next call to @p op to return false. //Forzar que la próxima operación falle
    //simular errores de hardware (NACK de I2C, timeouts, etc.) sin hardware real.
    void injectFailure(BleOperation op) {
        std::lock_guard<std::mutex> lock(mutex_);
        fail_next_ = op;
        has_pending_failure_ = true;
    }

    /// Advance the simulated clock by @p ms milliseconds. //Avanzar el reloj simulado
    //Para probar comportamiento que depende del tiempo
    // lo necesitas para probar lógica dependiente del tiempo sin esperar tiempo real. Es la misma idea del time_scale=0.0 que tenías en TCommand.
    void advanceTime(uint64_t ms) {
        current_millis_.fetch_add(ms);
    }

 private:
    mutable std::mutex mutex_;
    // Hardware radio status (not to be confused with BleState in the
    // state machine layer).
    bool radio_is_advertising_{false}; //¿El radio está anunciándose?
    bool link_is_active_{false}; //¿Hay conexión activa?
    uint32_t advertising_start_count_{0}; //Cuántas veces se intentó startAdvertising
    uint32_t advertising_stop_count_{0}; //Cuántas veces se intentó stopAdvertising
    uint32_t accept_connection_count_{0}; //Cuántas veces se intentó acceptConnection
    uint32_t disconnect_count_{0}; //Cuántas veces se intentó disconnect

    //Es una variable que guarda CUÁL operación debe fallar la próxima vez.
    BleOperation fail_next_{BleOperation::StartAdvertising}; //qué operación va a fallar la proxima vez
    bool has_pending_failure_{false}; //¿hay un fallo pendiente?

    std::atomic<uint64_t> current_millis_{0}; //El reloj simulado (en ms)
    /*
    current_millis_ es atomic porque se modifica/lee fuera del mutex. Atomic es más rápido y suficiente para una variable independiente.
    */
};

}  // namespace tble::hal