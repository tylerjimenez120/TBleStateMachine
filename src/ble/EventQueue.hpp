/**
 * @file EventQueue.hpp
 * @brief Thread-safe bounded FIFO of BleEvent values.
 *
 * Heap-free, statically sized via template parameter. Producers (user
 * code, hardware ISR, timers) push events; the state machine consumes
 * them one at a time. Uses std::condition_variable so a consumer can
 * block efficiently waiting for events instead of busy-polling.
 *
 * In a real RTOS this would be replaced by xQueueSend/xQueueReceive
 * (FreeRTOS) which encapsulate the same producer-consumer pattern at
 * the kernel level.
 */
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

#include "BleEvent.hpp"

namespace tble {

/**
 * @brief Fixed-capacity, thread-safe FIFO of BleEvent values.
 *
 * @tparam Capacity Maximum number of events the queue can hold.
 */
template <std::size_t Capacity>
class EventQueue {
 public:
    EventQueue() = default;

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue(EventQueue&&) = delete;
    EventQueue& operator=(EventQueue&&) = delete;

    /**
     * @brief Non-blocking push. Returns false if the queue is full or
     *        shut down. Safe to call from ISR context.
     */
    bool tryPush(BleEvent event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ >= Capacity || shutdown_.load()) return false;
        buffer_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Blocking pop. Waits until an event is available or shutdown.
     * @return The next event, or std::nullopt if shut down while empty.
     */
    std::optional<BleEvent> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] {
            return size_ > 0 || shutdown_.load();
        });
        if (size_ == 0) return std::nullopt;
        BleEvent event = buffer_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        return event;
    }

    /**
     * @brief Wake up all waiters and prevent further pushes.
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_.store(true);
        }
        not_empty_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    bool isShutdown() const noexcept { return shutdown_.load(); }

 private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::array<BleEvent, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
    std::atomic<bool> shutdown_{false};
};

}  // namespace tble


/*
                         ┌──────────────────────────┐
                         │      EventQueue          │
                         │  ┌────────────────────┐  │
   PRODUCTOR             │  │ buffer_ [0|1|2|3]  │  │              CONSUMIDOR
   ─────────             │  └────────────────────┘  │              ──────────
                         │     ▲              ▲     │
                         │     │              │     │
   tryPush(event) ──────►│   tail_         head_    │──────► pop() ──► event
                         │  (mete)         (saca)   │
                         │                          │
                         │  size_   shutdown_       │
                         └──────────────────────────┘
                                    ▲
                                    │
                              mutex_ + cv
                            (sincronización)


mete y avanza -> push
saca y avanza -> pop

tail → próxima posición libre (dónde se mete)
head → posición del más viejo (de dónde se saca)
head == tail → ambiguo (vacío o lleno) → desambigua size_
                            /**
 * @file EventQueue.hpp
 * @brief Thread-safe bounded FIFO of BleEvent values.
 *
 * Heap-free, statically sized via template parameter. Producers (user
 * code, hardware ISR, timers) push events; the state machine consumes
 * them one at a time. Uses std::condition_variable so a consumer can
 * block efficiently waiting for events instead of busy-polling.
 *
 * In a real RTOS this would be replaced by xQueueSend/xQueueReceive
 * (FreeRTOS) which encapsulate the same producer-consumer pattern at
 * the kernel level.
 */
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

#include "BleEvent.hpp"

namespace tble {

/**
 * @brief Fixed-capacity, thread-safe FIFO of BleEvent values.
 *
 * @tparam Capacity Maximum number of events the queue can hold.
 */
template <std::size_t Capacity>
class EventQueue {
 public:
    EventQueue() = default;

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue(EventQueue&&) = delete;
    EventQueue& operator=(EventQueue&&) = delete;

    /**
     * @brief Non-blocking push. Returns false if the queue is full or
     *        shut down. Safe to call from ISR context.
     */
    bool tryPush(BleEvent event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ >= Capacity || shutdown_.load()) return false;
        buffer_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Blocking pop. Waits until an event is available or shutdown.
     * @return The next event, or std::nullopt if shut down while empty.
     */
    std::optional<BleEvent> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] {
            return size_ > 0 || shutdown_.load();
        });
        if (size_ == 0) return std::nullopt;
        BleEvent event = buffer_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        return event;
    }

    /**
     * @brief Wake up all waiters and prevent further pushes.
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_.store(true);
        }
        not_empty_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    bool isShutdown() const noexcept { return shutdown_.load(); }

 private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::array<BleEvent, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
    std::atomic<bool> shutdown_{false};
};

}  // namespace tble


/*
                         ┌──────────────────────────┐
                         │      EventQueue          │
                         │  ┌────────────────────┐  │
   PRODUCER              │  │ buffer_ [0|1|2|3]  │  │              CONSUMER
   ────────              │  └────────────────────┘  │              ────────
                         │     ▲              ▲     │
                         │     │              │     │
   tryPush(event) ──────►│   tail_         head_    │──────► pop() ──► event
                         │  (writes)       (reads)  │
                         │                          │
                         │  size_   shutdown_       │
                         └──────────────────────────┘
                                    ▲
                                    │
                              mutex_ + cv
                            (synchronization)


write and advance -> push
read and advance  -> pop

tail → next free slot (where to write)
head → oldest position (where to read from)
head == tail → ambiguous (empty or full) → disambiguated by size_
                            */