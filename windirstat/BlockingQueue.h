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
    std::vector<std::jthread> m_Threads;
    std::deque<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Pushed;
    std::condition_variable m_Waiting;
    unsigned int m_TotalWorkerThreads = 1;
    unsigned int m_WorkersWaiting = 0;
    unsigned int m_StopReason = 0;
    bool m_Started = false;
    bool m_Suspended = false;
    bool m_Cancelled = false;
    bool m_ExitOnAllIdle = true;

    bool AllThreadsIdling() const
    {
        return m_TotalWorkerThreads == m_WorkersWaiting;
    }

public:
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;
    BlockingQueue& operator=(BlockingQueue&&) = delete;
    ~BlockingQueue() = default;
    BlockingQueue(bool exitOnAllIdle = true) : m_ExitOnAllIdle(exitOnAllIdle) {};

    void ThreadWrapper(const std::function<void()>& callback)
    {
        try
        {
            callback();
        }
        catch (std::exception&)
        {
            // Exception caught from a long-running task or cancellation
            std::scoped_lock lock(m_Mutex);
            m_WorkersWaiting++;
            m_Waiting.notify_all();
        }
    }

    void StartThreads(const unsigned int workerThreads, const std::function<void()>& callback)
    {
        ResetQueue(workerThreads, false);

        for (auto worker = 0u; worker < m_TotalWorkerThreads; worker++)
        {
            m_Threads.emplace_back(&BlockingQueue::ThreadWrapper, this, callback);
        }
    }

    void Push(T const& value)
    {
        // Push another entry onto the queue
        std::scoped_lock lock(m_Mutex);
        m_Queue.push_front(value);
        m_Pushed.notify_one();
    }

    std::optional<T> Pop()
    {
        // Record that the worker is waiting for an item until
        // the queue has something in it and we are not suspended
        std::unique_lock lock(m_Mutex);
        m_WorkersWaiting++;
        m_Waiting.notify_all();

        // Check if all workers are waiting and queue is empty - time to exit
        if (m_Started && AllThreadsIdling() && m_Queue.empty() && m_ExitOnAllIdle)
        {
            m_Cancelled = true;
            m_Pushed.notify_all();
            return std::nullopt;
        }

        m_Pushed.wait(lock, [&]
        {
            return !m_Suspended && !m_Queue.empty() || m_Cancelled;
        });

        if (m_Cancelled)
        {
            // Abort and signal other threads
            m_Pushed.notify_all();
            return std::nullopt;
        }

        // Worker now has something to work on so pop it off the queue
        m_WorkersWaiting--;
        m_Started = true;
        T i = m_Queue.front();
        m_Queue.pop_front();
        return i;
    }

    void PushIfNotQueued(T const& value)
    {
        std::scoped_lock lock(m_Mutex);
        if (std::ranges::find(m_Queue, value) != m_Queue.end()) return;
        m_Queue.push_back(value);
        m_Pushed.notify_one();
    }

    void WaitIfSuspended()
    {
        if (!m_Suspended) return;

        // wait until not suspended or its cancelled
        std::unique_lock lock(m_Mutex);
        m_WorkersWaiting++;
        m_Waiting.notify_all();
        m_Waiting.wait(lock, [&]
        {
            return !m_Suspended || m_Cancelled;
        });
        m_WorkersWaiting--;

        // if cancelled then throw to terminate current task
        if (m_Cancelled)
        {
            throw std::exception(__FUNCTION__);
        }
    }

    int WaitForCompletion()
    {
        // Wait for all workers threads to be idled or cancelled
        std::unique_lock lock(m_Mutex);
        m_Waiting.wait(lock, [&]
        {
            return m_Started && !m_Suspended && AllThreadsIdling() && m_Queue.empty() || m_Cancelled;
        });

        return m_StopReason;
    }

    void CancelExecution(const int stopReason = -1)
    {
        // Start cancellation process
        if (stopReason != -1) m_StopReason = stopReason;
        m_Cancelled = true;
        m_Waiting.notify_all();
        m_Pushed.notify_all();

        // Wait for threads to complete
        for (auto& thread : m_Threads)
        {
            thread.join();
        }

        // Cleanup
        ResetQueue(m_TotalWorkerThreads);
    }

    bool IsSuspended() const
    {
        return m_Started && m_Suspended;
    }

    void SuspendExecution(const bool clearQueue = false)
    {
        if (!m_Started) return;
        std::unique_lock lock(m_Mutex);
        m_Suspended = true;
        m_Waiting.notify_all();
        m_Waiting.wait(lock, [&]
        {
            return AllThreadsIdling();
        });
        if (clearQueue) m_Queue.clear();
    }

    void ResumeExecution()
    {
        std::scoped_lock lock(m_Mutex);
        m_Suspended = false;
        m_Waiting.notify_all();
        m_Pushed.notify_all();
    }

    void ResetQueue(const int totalWorkerThreads, const bool clearQueue = true)
    {
        std::scoped_lock lock(m_Mutex);
        m_WorkersWaiting = 0;
        m_Suspended = false;
        m_Started = false;
        m_Cancelled = false;
        m_TotalWorkerThreads = totalWorkerThreads;
        m_Threads.clear();
        m_Threads.reserve(m_TotalWorkerThreads);
        if (clearQueue) m_Queue.clear();
    }
};

template<typename T>
class SingleConsumerQueue
{
    struct Node
    {
        std::atomic<Node*> next{ nullptr };
        T data;

        Node() = default;

        template<typename U>
        explicit Node(U&& value) : data(std::forward<U>(value)) {}
    };

    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier
    alignas(std::hardware_destructive_interference_size) std::atomic<Node*> m_Head;
    alignas(std::hardware_destructive_interference_size) Node* m_Tail;
    #pragma warning(pop)

public:
    SingleConsumerQueue()
    {
        Node* dummy = new Node();
        m_Tail = dummy;
        m_Head.store(dummy, std::memory_order_relaxed);
    }

    ~SingleConsumerQueue()
    {
        for (T tmp; pop(tmp););
        delete m_Tail;
    }

    template<typename U>
    void push(U&& item)
    {
        Node* node = new Node(std::forward<U>(item));
        Node* prev = m_Head.exchange(node, std::memory_order_release);
        prev->next.store(node, std::memory_order_release);
    }

    [[nodiscard]] bool pop(T& item) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        Node* t = m_Tail;
        Node* next = t->next.load(std::memory_order_acquire);

        if (next == nullptr)
            return false;

        item = std::move(next->data);
        m_Tail = next;

        delete t;
        return true;
    }

    // Only valid for consumer
    [[nodiscard]] bool empty() const noexcept
    {
        return m_Tail->next.load(std::memory_order_acquire) == nullptr;
    }

    // Clear out queue (unsafe with concurrent producers)
    void clear() noexcept
    {
        for (T tmp; pop(tmp););

        Node* old = m_Tail; // One dummy remains
        Node* dummy = new Node();

        m_Tail = dummy;
        m_Head.store(dummy, std::memory_order_release);

        delete old;
    }
};
