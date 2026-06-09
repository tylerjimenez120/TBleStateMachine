/**
 * @file test_event_queue.cpp
 * @brief Unit tests for tble::EventQueue.
 */
#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "ble/EventQueue.hpp"

using tble::BleEvent;
using tble::BleEventType;
using tble::EventQueue;

TEST(EventQueueTest, PushPopFifoOrder) {
    EventQueue<8> q;

    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::StartAdvertising}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::ConnectionRequest}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::Disconnect}));

    EXPECT_EQ(q.size(), 3u);

    auto a = q.pop();
    auto b = q.pop();
    auto c = q.pop();

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());

    EXPECT_EQ(a->type, BleEventType::StartAdvertising);
    EXPECT_EQ(b->type, BleEventType::ConnectionRequest);
    EXPECT_EQ(c->type, BleEventType::Disconnect);
}

TEST(EventQueueTest, TryPushFailsWhenFull) {
    EventQueue<2> q;

    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::StartAdvertising}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::StopAdvertising}));
    EXPECT_FALSE(q.tryPush(BleEvent{BleEventType::Timeout}));

    EXPECT_EQ(q.size(), 2u);
}

TEST(EventQueueTest, ShutdownReleasesBlockedPop) {
    EventQueue<4> q;
    std::atomic<bool> popped{false};

    std::thread consumer([&] {
        auto e = q.pop();
        EXPECT_FALSE(e.has_value());  // nullopt on shutdown
        popped.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(popped.load());

    q.shutdown();
    consumer.join();
    EXPECT_TRUE(popped.load());
}

TEST(EventQueueTest, CircularBufferWrapsAround) {
    EventQueue<3> q;

    // Llena la cola
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::StartAdvertising}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::StopAdvertising}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::Timeout}));

    // Saca dos
    auto a = q.pop();
    auto b = q.pop();
    EXPECT_EQ(a->type, BleEventType::StartAdvertising);
    EXPECT_EQ(b->type, BleEventType::StopAdvertising);

    // Mete dos más (el tail debe envolverse al inicio del array)
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::Disconnect}));
    ASSERT_TRUE(q.tryPush(BleEvent{BleEventType::LinkLoss}));

    EXPECT_EQ(q.size(), 3u);

    // Verifica orden FIFO después del wrap
    EXPECT_EQ(q.pop()->type, BleEventType::Timeout);
    EXPECT_EQ(q.pop()->type, BleEventType::Disconnect);
    EXPECT_EQ(q.pop()->type, BleEventType::LinkLoss);
}

TEST(EventQueueTest, MultiProducerSingleConsumer) {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 100;

    EventQueue<64> q;
    std::atomic<int> consumed{0};

    std::thread consumer([&] {
        while (true) {
            auto e = q.pop();
            if (!e.has_value()) break;
            consumed.fetch_add(1);
        }
    });

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < kPerProducer; ++i) {
                while (!q.tryPush(BleEvent{BleEventType::Timeout})) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t : producers) t.join();

    // Esperar drenaje
    while (q.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    q.shutdown();
    consumer.join();

    EXPECT_EQ(consumed.load(), kProducers * kPerProducer);
}