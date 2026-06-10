/**
 * @file test_state_machine.cpp
 * @brief Unit tests for tble::BleStateMachine.
 *
 * Uses minimal FakeState implementations to verify the orchestration
 * logic (start/stop, event delivery, transitions). Real state behavior
 * is tested separately in PR #4.
 */
#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "ble/BleStateMachine.hpp"

using tble::BleEvent;
using tble::BleEventType;
using tble::BleState;
using tble::BleStateMachine;
using tble::IState;

namespace {

/// Minimal state that counts enter/exit/handleEvent calls.
/// Transitions to `next_state_` when it receives the configured event.
class FakeState final : public IState {
 public:
    FakeState(BleState id, BleEventType trigger)
        : id_{id}, trigger_{trigger} {}

    void setNext(IState* next) { next_state_ = next; }

    void enter() override { enter_count_.fetch_add(1); }
    void exit() override { exit_count_.fetch_add(1); }

    IState* handleEvent(BleEvent event) override {
        handle_count_.fetch_add(1);
        if (event.type == trigger_ && next_state_ != nullptr) {
            return next_state_;
        }
        return nullptr;
    }

    BleState id() const noexcept override { return id_; }

    uint32_t enterCount() const noexcept { return enter_count_.load(); }
    uint32_t exitCount() const noexcept { return exit_count_.load(); }
    uint32_t handleCount() const noexcept { return handle_count_.load(); }

 private:
    BleState id_;
    BleEventType trigger_;
    IState* next_state_{nullptr};
    std::atomic<uint32_t> enter_count_{0};
    std::atomic<uint32_t> exit_count_{0};
    std::atomic<uint32_t> handle_count_{0};
};

void waitABit() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

}  // namespace

TEST(BleStateMachineTest, StartCallsEnterOnInitialState) {
    FakeState s{BleState::Disconnected, BleEventType::Reset};

    BleStateMachine<8> sm{&s};
    EXPECT_EQ(s.enterCount(), 0u);

    sm.start();
    waitABit();
    sm.stop();

    EXPECT_EQ(s.enterCount(), 1u);
    EXPECT_EQ(s.exitCount(), 0u);  // never transitioned out
}

TEST(BleStateMachineTest, EventIsDeliveredToCurrentState) {
    FakeState s{BleState::Disconnected, BleEventType::Reset};

    BleStateMachine<8> sm{&s};
    sm.start();

    EXPECT_TRUE(sm.pushEvent(BleEvent{BleEventType::Timeout}));
    waitABit();

    EXPECT_EQ(s.handleCount(), 1u);

    sm.stop();
}

TEST(BleStateMachineTest, TransitionExecutesExitAndEnter) {
    FakeState a{BleState::Disconnected, BleEventType::StartAdvertising};
    FakeState b{BleState::Advertising, BleEventType::Reset};

    a.setNext(&b);  // a transitions to b on StartAdvertising

    BleStateMachine<8> sm{&a};
    sm.start();

    EXPECT_EQ(a.enterCount(), 1u);

    // Trigger the transition
    sm.pushEvent(BleEvent{BleEventType::StartAdvertising});
    waitABit();

    EXPECT_EQ(a.exitCount(), 1u);   // a exited
    EXPECT_EQ(b.enterCount(), 1u);  // b entered
    EXPECT_EQ(sm.currentStateId(), BleState::Advertising);

    sm.stop();
}

TEST(BleStateMachineTest, EventThatDoesNotTriggerTransitionStays) {
    FakeState a{BleState::Disconnected, BleEventType::StartAdvertising};
    FakeState b{BleState::Advertising, BleEventType::Reset};

    a.setNext(&b);

    BleStateMachine<8> sm{&a};
    sm.start();

    // Send an event that does NOT match a.trigger_
    sm.pushEvent(BleEvent{BleEventType::Timeout});
    waitABit();

    EXPECT_EQ(a.handleCount(), 1u);   // event was processed
    EXPECT_EQ(a.exitCount(), 0u);     // but no transition
    EXPECT_EQ(b.enterCount(), 0u);
    EXPECT_EQ(sm.currentStateId(), BleState::Disconnected);

    sm.stop();
}

TEST(BleStateMachineTest, StopJoinsWorkerCleanly) {
    FakeState s{BleState::Disconnected, BleEventType::Reset};

    BleStateMachine<8> sm{&s};
    sm.start();
    sm.stop();
    // If stop() did not shut down cleanly, the test would hang or crash.
    SUCCEED();
}

TEST(BleStateMachineTest, DoubleStartIsIdempotent) {
    FakeState s{BleState::Disconnected, BleEventType::Reset};

    BleStateMachine<8> sm{&s};
    sm.start();
    sm.start();  // second call is a no-op
    waitABit();

    EXPECT_EQ(s.enterCount(), 1u);  // enter() called only once

    sm.stop();
}