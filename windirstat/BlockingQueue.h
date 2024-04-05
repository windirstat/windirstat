#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue
{
    std::deque<T> q;
    std::mutex x;
    std::condition_variable pushed;
    std::condition_variable waiting;
    std::condition_variable popped;
    unsigned int m_initial_workers;
    unsigned int m_workers_waiting = 0;
    bool m_started = false;
    bool m_suspended = false;
    bool m_draining = false;

public:
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;
    BlockingQueue& operator=(BlockingQueue&&) = delete;
    ~BlockingQueue() = default;
    BlockingQueue(const unsigned int workers = 1) :
        m_initial_workers(workers) {}

    void push(T const& value, bool back = true)
    {
        // Push another entry onto the queue
        std::lock_guard lock(x);
        if (m_draining) return;
        back ? q.push_back(value) : q.push_front(value);
        pushed.notify_one();
    }

    T pop(bool front = true)
    {
        // Record the worker is waiting for an item until
        // the queue has something in it and we are not suspended
        std::unique_lock lock(x);
        m_workers_waiting++;
        waiting.notify_all();
        pushed.wait(lock, [&]
        {
            return (!m_suspended || m_draining) && !q.empty();
        });

        // Worker now has something to work on so pop it off the queue
        if (!m_draining) m_started = true;
        m_workers_waiting--;
        T& i = front ? q.front() : q.back();
        front ? q.pop_front() : q.pop_back();
        popped.notify_one();
        return i;
    }

    bool wait_for_all()
    {
        // Wait for all workers threads to be
        // waiting for more work to do 
        std::unique_lock lock(x);
        waiting.wait(lock, [&]
        {
            return m_started && m_draining && q.empty() ||
                !m_suspended && m_workers_waiting == m_initial_workers;
        });
        return m_draining;
    }

    bool drain(T drain_object)
    {
        // Stop current proocessing
        suspend(true);

        // Return early if queue is already draining or never started
        std::unique_lock lock(x);
        if (!m_started || m_draining)
        {
            return false;
        }

        // Start draining process by feeding some special
        // draining objects into the queue (implementation dependent)
        m_draining = true;
        q.clear();
        for (unsigned int i = 0; i < m_initial_workers; i++)
        {
            q.push_back(drain_object);
            pushed.notify_one();
        }

        // Wait until draining objects have been processed
        popped.wait(lock, [&]
        {
            return q.empty();
        });
        waiting.notify_all();
        return true;
    }

    bool has_items() const
    {
        return !q.empty();
    }

    bool is_suspended() const
    {
        return m_suspended;
    }

    void suspend(bool wait)
    {
        std::unique_lock lock(x);
        if (m_suspended || m_draining || !m_started) return;
        m_suspended = true;
        if (wait) waiting.wait(lock, [&]
        {
            return m_workers_waiting == m_initial_workers;
        });
    }

    void resume()
    {
        std::lock_guard lock(x);
        m_started = false;
        m_suspended = false;
        m_draining = false;
        pushed.notify_all();
    }

    void reset(int initial_workers = -1)
    {
        std::lock_guard lock(x);
        q.clear();
        m_workers_waiting = 0;
        m_suspended = false;
        m_started = false;
        m_draining = false;
        if (initial_workers > 0)
        {
            m_initial_workers = initial_workers;
        }
    }
};

