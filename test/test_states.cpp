/**
 * @file test_states.cpp
 * @brief Integration tests for the concrete BLE states.
 *
 * Wires up the full system (HAL + state machine + 5 concrete states)
 * and verifies that real events drive the correct transitions and
 * HAL calls.
 */
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "ble/BleStateMachine.hpp"
#include "hal/MockBleController.hpp"
#include "states/AdvertisingState.hpp"
#include "states/ConnectedState.hpp"
#include "states/ConnectingState.hpp"
#include "states/DisconnectedState.hpp"
#include "states/ErrorState.hpp"

using tble::BleEvent;
using tble::BleEventType;
using tble::BleState;
using tble::BleStateMachine;
using tble::hal::MockBleController;
using tble::states::AdvertisingState;
using tble::states::ConnectedState;
using tble::states::ConnectingState;
using tble::states::DisconnectedState;
using tble::states::ErrorState;

namespace {

/// Wait briefly for the worker thread to process posted events.
void waitABit() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

/**
 * @brief Test fixture that wires up the full system.
 *
 * Creates HAL, all 5 states, connects them, and provides a state machine
 * pointing at Disconnected as initial state.
 */
struct BleSystem {
    MockBleController hal{};
    DisconnectedState disconnected{hal};
    AdvertisingState  advertising{hal};
    ConnectingState   connecting{hal};
    ConnectedState    connected{hal};
    ErrorState        error{hal};

    BleStateMachine<16> sm{&disconnected};

    BleSystem() {
        // Wire up all transitions
        disconnected.setAdvertising(&advertising);

        advertising.setDisconnected(&disconnected);
        advertising.setConnecting(&connecting);

        connecting.setConnected(&connected);
        connecting.setError(&error);

        connected.setDisconnected(&disconnected);

        error.setDisconnected(&disconnected);
    }
};

}  // namespace

TEST(BleStatesTest, StartsInDisconnected) {
    BleSystem sys;
    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);
}

TEST(BleStatesTest, DisconnectedToAdvertising) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Advertising);
    EXPECT_TRUE(sys.hal.radioIsAdvertising());
    EXPECT_EQ(sys.hal.advertisingStartCount(), 1u);

    sys.sm.stop();
}

TEST(BleStatesTest, AdvertisingToConnectingOnConnectionRequest) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Connecting);
    EXPECT_TRUE(sys.hal.linkIsActive());
    EXPECT_FALSE(sys.hal.radioIsAdvertising());

    sys.sm.stop();
}

TEST(BleStatesTest, ConnectingToConnectedOnEstablished) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionEstablished});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Connected);

    sys.sm.stop();
}

TEST(BleStatesTest, ConnectingToErrorOnTimeout) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    sys.sm.pushEvent(BleEvent{BleEventType::Timeout});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Error);

    sys.sm.stop();
}

TEST(BleStatesTest, ConnectedToDisconnectedOnDisconnect) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionEstablished});
    sys.sm.pushEvent(BleEvent{BleEventType::Disconnect});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);
    EXPECT_FALSE(sys.hal.linkIsActive());
    EXPECT_EQ(sys.hal.disconnectCount(), 1u);

    sys.sm.stop();
}

TEST(BleStatesTest, ConnectedToDisconnectedOnLinkLossDoesNotCallHal) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionEstablished});
    sys.sm.pushEvent(BleEvent{BleEventType::LinkLoss});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);
    EXPECT_EQ(sys.hal.disconnectCount(), 0u);  // LinkLoss doesn't call HAL

    sys.sm.stop();
}

TEST(BleStatesTest, ErrorToDisconnectedOnReset) {
    BleSystem sys;
    sys.sm.start();

    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    sys.sm.pushEvent(BleEvent{BleEventType::ConnectionRequest});
    sys.sm.pushEvent(BleEvent{BleEventType::Timeout});
    sys.sm.pushEvent(BleEvent{BleEventType::Reset});
    waitABit();

    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);

    sys.sm.stop();
}

TEST(BleStatesTest, HardwareFailureKeepsCurrentState) {
    BleSystem sys;
    sys.sm.start();

    // Inject failure on startAdvertising
    sys.hal.injectFailure(tble::hal::BleOperation::StartAdvertising);
    sys.sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    waitABit();

    // State should stay in Disconnected because HAL failed
    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);
    EXPECT_FALSE(sys.hal.radioIsAdvertising());

    sys.sm.stop();
}

TEST(BleStatesTest, IrrelevantEventIsIgnored) {
    BleSystem sys;
    sys.sm.start();

    // In Disconnected, a Disconnect event makes no sense
    sys.sm.pushEvent(BleEvent{BleEventType::Disconnect});
    waitABit();

    // State machine stays in Disconnected
    EXPECT_EQ(sys.sm.currentStateId(), BleState::Disconnected);

    sys.sm.stop();
}