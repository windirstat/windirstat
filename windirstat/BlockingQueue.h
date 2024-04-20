#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue
{
    std::deque<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Pushed;
    std::condition_variable m_Waiting;
    std::condition_variable m_Popped;
    unsigned int m_InitialWorkers = 1;
    unsigned int m_WorkersWaiting = 0;
    bool m_Started = false;
    bool m_Suspended = false;
    bool m_Draining = false;

public:
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;
    BlockingQueue& operator=(BlockingQueue&&) = delete;
    ~BlockingQueue() = default;
    BlockingQueue(const unsigned int workers = 1) :
        m_InitialWorkers(workers) {}

    void push(T const& value, bool back = true)
    {
        // Push another entry onto the queue
        std::lock_guard lock(m_Mutex);
        if (m_Draining) return;
        back ? m_Queue.push_back(value) : m_Queue.push_front(value);
        m_Pushed.notify_one();
    }

    T pop(bool front = true)
    {
        // Record the worker is m_Waiting for an item until
        // the queue has something in it and we are not suspended
        std::unique_lock lock(m_Mutex);
        m_WorkersWaiting++;
        m_Waiting.notify_all();
        m_Pushed.wait(lock, [&]
        {
            return (!m_Suspended || m_Draining) && !m_Queue.empty();
        });

        // Worker now has something to work on so pop it off the queue
        if (!m_Draining) m_Started = true;
        m_WorkersWaiting--;
        T& i = front ? m_Queue.front() : m_Queue.back();
        front ? m_Queue.pop_front() : m_Queue.pop_back();
        m_Popped.notify_one();
        return i;
    }

    bool waitForAll()
    {
        // Wait for all workers threads to be
        // m_Waiting for more work to do 
        std::unique_lock lock(m_Mutex);
        m_Waiting.wait(lock, [&]
        {
            return m_Started && m_Draining && m_Queue.empty() ||
                !m_Suspended && m_WorkersWaiting == m_InitialWorkers;
        });
        return m_Draining;
    }

    bool drain(T drainObject)
    {
        // Stop current proocessing
        suspend(true);

        // Return early if queue is already draining or never started
        std::unique_lock lock(m_Mutex);
        if (!m_Started || m_Draining)
        {
            return false;
        }

        // Start draining process by feeding some special
        // draining objects into the queue (implementation dependent)
        m_Draining = true;
        m_Queue.clear();
        for (unsigned int i = 0; i < m_InitialWorkers; i++)
        {
            m_Queue.push_back(drainObject);
            m_Pushed.notify_one();
        }

        // Wait until draining objects have been processed
        m_Popped.wait(lock, [&]
        {
            return m_Queue.empty();
        });
        m_Waiting.notify_all();
        return true;
    }

    bool hasItems() const
    {
        return !m_Queue.empty();
    }

    bool isSuspended() const
    {
        return m_Suspended;
    }

    void suspend(const bool wait)
    {
        std::unique_lock lock(m_Mutex);
        if (m_Suspended || m_Draining || !m_Started) return;
        m_Suspended = true;
        if (wait) m_Waiting.wait(lock, [&]
        {
            return m_WorkersWaiting == m_InitialWorkers;
        });
    }

    void resume()
    {
        std::lock_guard lock(m_Mutex);
        m_Started = false;
        m_Suspended = false;
        m_Draining = false;
        m_Pushed.notify_all();
    }

    void reset(const int initialWorkers = -1)
    {
        std::lock_guard lock(m_Mutex);
        m_Queue.clear();
        m_WorkersWaiting = 0;
        m_Suspended = false;
        m_Started = false;
        m_Draining = false;
        if (initialWorkers > 0)
        {
            m_InitialWorkers = initialWorkers;
        }
    }
};

