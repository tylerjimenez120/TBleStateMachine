/**
 * @file test_mock_ble_controller.cpp
 * @brief Unit tests for tble::hal::MockBleController.
 *
 * Validates the radio state flags, call counters, failure injection,
 * and manual clock. These are the foundations that the state machine
 * tests (PR #2+) will rely on.
 */
#include <gtest/gtest.h>

#include "hal/MockBleController.hpp"

using tble::hal::BleOperation;
using tble::hal::MockBleController;

TEST(MockBleControllerTest, StartAdvertisingSetsRadioFlag) {
    MockBleController mock;

    EXPECT_FALSE(mock.radioIsAdvertising());
    EXPECT_TRUE(mock.startAdvertising());
    EXPECT_TRUE(mock.radioIsAdvertising());
    EXPECT_EQ(mock.advertisingStartCount(), 1u);
}

TEST(MockBleControllerTest, StopAdvertisingClearsRadioFlag) {
    MockBleController mock;

    mock.startAdvertising();
    ASSERT_TRUE(mock.radioIsAdvertising());

    EXPECT_TRUE(mock.stopAdvertising());
    EXPECT_FALSE(mock.radioIsAdvertising());
    EXPECT_EQ(mock.advertisingStopCount(), 1u);
}

TEST(MockBleControllerTest, AcceptConnectionTransitionsBothFlags) {
    MockBleController mock;

    mock.startAdvertising();
    ASSERT_TRUE(mock.radioIsAdvertising());
    ASSERT_FALSE(mock.linkIsActive());

    EXPECT_TRUE(mock.acceptConnection());
    EXPECT_FALSE(mock.radioIsAdvertising());  // adv off
    EXPECT_TRUE(mock.linkIsActive());         // link on
    EXPECT_EQ(mock.acceptConnectionCount(), 1u);
}

TEST(MockBleControllerTest, DisconnectClearsLink) {
    MockBleController mock;

    mock.startAdvertising();
    mock.acceptConnection();
    ASSERT_TRUE(mock.linkIsActive());

    EXPECT_TRUE(mock.disconnect());
    EXPECT_FALSE(mock.linkIsActive());
    EXPECT_EQ(mock.disconnectCount(), 1u);
}

TEST(MockBleControllerTest, InjectedFailureMakesCallReturnFalse) {
    MockBleController mock;

    mock.injectFailure(BleOperation::StartAdvertising);

    EXPECT_FALSE(mock.startAdvertising());      // fails
    EXPECT_FALSE(mock.radioIsAdvertising());    // hardware did NOT enter advertising
    EXPECT_EQ(mock.advertisingStartCount(), 1u); // but the attempt was counted
}

TEST(MockBleControllerTest, FailureIsConsumedAfterOneCall) {
    MockBleController mock;

    mock.injectFailure(BleOperation::StartAdvertising);

    EXPECT_FALSE(mock.startAdvertising());      // first call fails
    EXPECT_TRUE(mock.startAdvertising());       // second call succeeds
    EXPECT_TRUE(mock.radioIsAdvertising());
    EXPECT_EQ(mock.advertisingStartCount(), 2u);
}

TEST(MockBleControllerTest, AdvanceTimeAdvancesTheClock) {
    MockBleController mock;

    EXPECT_EQ(mock.millis(), 0u);
    mock.advanceTime(500);
    EXPECT_EQ(mock.millis(), 500u);
    mock.advanceTime(1500);
    EXPECT_EQ(mock.millis(), 2000u);
}