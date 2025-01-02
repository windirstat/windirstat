// BlockingQueue.h - Functions used by WinDirStat.exe and setup.exe
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include <stack>
#include <mutex>
#include <condition_variable>
#include <functional>

template <typename T>
class BlockingQueue final
{
    std::vector<std::thread> m_Threads;
    std::deque<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Pushed;
    std::condition_variable m_Waiting;
    unsigned int m_TotalWorkerThreads = 1;
    unsigned int m_WorkersWaiting = 0;
    bool m_Started = false;
    bool m_Suspended = false;
    bool m_Cancelled = false;

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
    BlockingQueue() = default;

    void ThreadWrapper(const std::function<void()> & callback)
    {
        try
        {
            callback();
        }
        catch (std::exception&)
        {
            // caught from long running or cancelled
            std::lock_guard lock(m_Mutex);
            m_WorkersWaiting++;
            m_Waiting.notify_all();
        }
    }

    void StartThreads(const unsigned int workerThreads, const std::function<void()> & callback)
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
        std::lock_guard lock(m_Mutex);
        m_Queue.push_front(value);
        m_Pushed.notify_one();
    }

    T Pop()
    {
        // Record the worker is m_Waiting for an item until
        // the queue has something in it and we are not suspended
        std::unique_lock lock(m_Mutex);
        m_WorkersWaiting++;
        m_Waiting.notify_all();
        m_Pushed.wait(lock, [&]
        {
            return !m_Suspended && !m_Queue.empty() || m_Cancelled;
        });
        m_WorkersWaiting--;

        if (m_Cancelled)
        {
            // Mark we are in waiting mode again and abort
            throw std::exception(__FUNCTION__);
        }

        // Worker now has something to work on so pop it off the queue
        m_Started = true;
        T i = m_Queue.front();
        m_Queue.pop_front();
        return i;
    }

    void PushIfNotQueued(T const& value)
    {
        std::lock_guard lock(m_Mutex);
        if (std::ranges::find(m_Queue, value) != m_Queue.end()) return;
        m_Queue.push_back(value);
        m_Pushed.notify_one();
    }

    void WaitIfSuspended()
    {
        if (!m_Suspended) return;

        // wait until its not suspended or its cancelled
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

    void WaitForCompletion()
    {
        // Wait for all workers threads to be idled or cancelled
        std::unique_lock lock(m_Mutex);
        m_Waiting.wait(lock, [&]
        {
            return m_Started && !m_Suspended && AllThreadsIdling() && m_Queue.empty() || m_Cancelled;
        });
    }

    void CancelExecution()
    {
        // Start cancellation process
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
        std::lock_guard lock(m_Mutex);
        m_Suspended = false;
        m_Waiting.notify_all();
        m_Pushed.notify_all();
    }

    void ResetQueue(const int totalWorkerThreads, const bool clearQueue = true)
    {
        std::lock_guard lock(m_Mutex);
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

