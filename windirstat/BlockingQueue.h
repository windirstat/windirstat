// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"

template <typename T>
class BlockingQueue final
{
    std::vector<std::jthread> m_threads;
    std::deque<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_pushed;
    std::condition_variable m_waiting;
    unsigned int m_totalWorkerThreads = 1;
    unsigned int m_workersWaiting = 0;
    unsigned int m_stopReason = 0;
    bool m_started = false;
    bool m_suspended = false;
    bool m_cancelled = false;
    bool m_exitOnAllIdle = true;

    bool AllThreadsIdling() const
    {
        return m_totalWorkerThreads == m_workersWaiting;
    }

public:
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;
    BlockingQueue& operator=(BlockingQueue&&) = delete;
    ~BlockingQueue() = default;
    BlockingQueue(bool exitOnAllIdle = true) : m_exitOnAllIdle(exitOnAllIdle) {};

    void ThreadWrapper(const std::function<void()>& callback)
    {
        try
        {
            callback();
        }
        catch (std::exception&)
        {
            // Exception caught from a long-running task or cancellation
            std::scoped_lock lock(m_mutex);
            m_workersWaiting++;
            m_waiting.notify_all();
        }
    }

    void StartThreads(const unsigned int workerThreads, const std::function<void()>& callback)
    {
        ResetQueue(workerThreads, false);

        for ([[maybe_unused]] const auto _ : std::views::iota(0u, m_totalWorkerThreads))
        {
            m_threads.emplace_back(&BlockingQueue::ThreadWrapper, this, callback);
        }
    }

    void Push(T const& value)
    {
        // Push another entry onto the queue
        std::scoped_lock lock(m_mutex);
        m_queue.push_front(value);
        m_pushed.notify_one();
    }

    std::optional<T> Pop()
    {
        // Record that the worker is waiting for an item until
        // the queue has something in it and we are not suspended
        std::unique_lock lock(m_mutex);
        m_workersWaiting++;
        m_waiting.notify_all();

        // Check if all workers are waiting and queue is empty - time to exit
        if (m_started && AllThreadsIdling() && m_queue.empty() && m_exitOnAllIdle)
        {
            m_cancelled = true;
            m_pushed.notify_all();
            return std::nullopt;
        }

        m_pushed.wait(lock, [&]
        {
            return !m_suspended && !m_queue.empty() || m_cancelled;
        });

        if (m_cancelled)
        {
            // Abort and signal other threads
            m_pushed.notify_all();
            return std::nullopt;
        }

        // Worker now has something to work on so pop it off the queue
        m_workersWaiting--;
        m_started = true;
        T i = m_queue.front();
        m_queue.pop_front();
        return i;
    }

    void PushIfNotQueued(T const& value)
    {
        std::scoped_lock lock(m_mutex);
        if (std::ranges::find(m_queue, value) != m_queue.end()) return;
        m_queue.push_back(value);
        m_pushed.notify_one();
    }

    void WaitIfSuspended()
    {
        if (!m_suspended) return;

        // wait until not suspended or its cancelled
        std::unique_lock lock(m_mutex);
        m_workersWaiting++;
        m_waiting.notify_all();
        m_waiting.wait(lock, [&]
        {
            return !m_suspended || m_cancelled;
        });
        m_workersWaiting--;

        // if cancelled then throw to terminate current task
        if (m_cancelled)
        {
            throw std::exception(__FUNCTION__);
        }
    }

    int WaitForCompletion()
    {
        // Wait for all workers threads to be idled or cancelled
        std::unique_lock lock(m_mutex);
        m_waiting.wait(lock, [&]
        {
            return m_started && !m_suspended && AllThreadsIdling() && m_queue.empty() || m_cancelled;
        });

        return m_stopReason;
    }

    void CancelExecution(const int stopReason = -1)
    {
        // Start cancellation process
        if (stopReason != -1) m_stopReason = stopReason;
        m_cancelled = true;
        m_waiting.notify_all();
        m_pushed.notify_all();

        // Wait for threads to complete
        for (auto& thread : m_threads)
        {
            thread.join();
        }

        // Cleanup
        ResetQueue(m_totalWorkerThreads);
    }

    void SuspendExecution(const bool clearQueue = false)
    {
        if (!m_started) return;
        std::unique_lock lock(m_mutex);
        m_suspended = true;
        m_waiting.notify_all();
        m_waiting.wait(lock, [&]
        {
            return AllThreadsIdling();
        });
        if (clearQueue) m_queue.clear();
    }

    void ResumeExecution()
    {
        std::scoped_lock lock(m_mutex);
        m_suspended = false;
        m_waiting.notify_all();
        m_pushed.notify_all();
    }

    void ResetQueue(const int totalWorkerThreads, const bool clearQueue = true)
    {
        std::scoped_lock lock(m_mutex);
        m_workersWaiting = 0;
        m_suspended = false;
        m_started = false;
        m_cancelled = false;
        m_totalWorkerThreads = totalWorkerThreads;
        m_threads.clear();
        m_threads.reserve(m_totalWorkerThreads);
        if (clearQueue) m_queue.clear();
    }
};

template<typename T>
class SingleConsumerQueue
{
    struct Node
    {
        std::atomic<Node*> next{ nullptr };
        T data{};

        Node() = default;

        template<typename U>
        explicit Node(U&& value) : data(std::forward<U>(value)) {}
    };

    alignas(std::hardware_destructive_interference_size) std::atomic<Node*> m_head;
    alignas(std::hardware_destructive_interference_size) Node* m_tail;

public:
    SingleConsumerQueue()
    {
        Node* dummy = new Node();
        m_tail = dummy;
        m_head.store(dummy, std::memory_order_relaxed);
    }

    ~SingleConsumerQueue()
    {
        for (T tmp; pop(tmp););
        delete m_tail;
    }

    template<typename U>
    void push(U&& item)
    {
        Node* node = new Node(std::forward<U>(item));
        Node* prev = m_head.exchange(node, std::memory_order_release);
        prev->next.store(node, std::memory_order_release);
    }

    [[nodiscard]] bool pop(T& item) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        Node* t = m_tail;
        Node* next = t->next.load(std::memory_order_acquire);

        if (next == nullptr)
            return false;

        item = std::move(next->data);
        m_tail = next;

        delete t;
        return true;
    }

    // Only valid for consumer
    [[nodiscard]] bool empty() const noexcept
    {
        return m_tail->next.load(std::memory_order_acquire) == nullptr;
    }

    // Clear out queue (unsafe with concurrent producers)
    void clear() noexcept
    {
        for (T tmp; pop(tmp););

        Node* old = m_tail; // One dummy remains
        Node* dummy = new Node();

        m_tail = dummy;
        m_head.store(dummy, std::memory_order_release);

        delete old;
    }
};
