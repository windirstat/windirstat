#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>

template <typename T>
class BlockingQueue
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
    bool m_Draining = false;

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
            // caught from long running or draining
            std::lock_guard lock(m_Mutex);
            m_WorkersWaiting++;
            m_Waiting.notify_all();
        }
    }

    void StartThreads(const unsigned int workerThreads, const std::function<void()> & callback)
    {
        ResetQueue(workerThreads);

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
            return (!m_Suspended && !m_Queue.empty()) || m_Draining;
        });
        m_WorkersWaiting--;

        if (m_Draining)
        {
            // Mark we are in waiting mode again and abort
            throw std::exception(__FUNCTION__);
        }

        // Worker now has something to work on so pop it off the queue
        m_Started = true;
        T& i = m_Queue.front();
        m_Queue.pop_front();
        return i;
    }

    void WaitIfSuspended()
    {
        if (!m_Suspended) return;

        // wait until its not suspended or its draining
        std::unique_lock lock(m_Mutex);
        m_WorkersWaiting++;
        m_Waiting.notify_all();
        m_Waiting.wait(lock, [&]
        {
            return !m_Suspended || m_Draining;
        });
        m_WorkersWaiting--;

        // if draining then throw to terminate current task
        if (m_Draining)
        {
            throw std::exception(__FUNCTION__);
        }
    }

    bool WaitForCompletionOrCancellation()
    {
        // Wait for all workers threads to be idled or draining
        std::unique_lock lock(m_Mutex);
        m_Waiting.wait(lock, [&]
        {
            return m_Started && !m_Suspended && AllThreadsIdling() || m_Draining;
        });
        return m_Draining;
    }

    void CancelExecution()
    {
        // Return early if queue is already draining or never started
        if (!m_Started || m_Draining)
        {
            return;
        }

        // Wait for queue to SuspendExecution first
        SuspendExecution();

        // Start draining process 
        m_Draining = true;
        m_Waiting.notify_all();
        m_Pushed.notify_all();

        // Wait for threads to complete
        for (auto& thread : m_Threads)
        {
            thread.join();
        }

        // Cleanup
        ResetQueue(m_TotalWorkerThreads);
        m_Queue.clear();
    }

    bool HasItems() const
    {
        return !m_Queue.empty();
    }

    bool IsSuspended() const
    {
        return m_Started && m_Suspended;
    }

    void SuspendExecution()
    {
        // Wait for all threads to idle
        if (CWnd * wnd = AfxGetMainWnd(); GetWindowThreadProcessId(
            wnd->m_hWnd, nullptr) == GetCurrentThreadId())
        {
            static auto waitMessage = RegisterWindowMessage(L"WinDirStatQueue");

            std::thread([wnd, this]() mutable
            {
                std::unique_lock lock(m_Mutex);
                m_Suspended = true;
                m_Waiting.notify_all();
                m_Waiting.wait(lock, [&]
                {
                    return AllThreadsIdling();
                });
                wnd->PostMessageW(waitMessage, 0, 0);
            }).detach();

            // Read all messages in this next loop, removing each message as we read it.
            CWaitCursor wc;
            for (MSG msg; ::GetMessage(&msg, nullptr, 0, 0); )
            {
                if (msg.message == waitMessage) break;
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
        else
        {
            std::unique_lock lock(m_Mutex);
            m_Suspended = true;
            m_Waiting.notify_all();
            m_Waiting.wait(lock, [&]
            {
                return AllThreadsIdling();
            });
        }
    }

    void ResumeExecution()
    {
        std::lock_guard lock(m_Mutex);
        m_Started = false;
        m_Suspended = false;
        m_Waiting.notify_all();
        m_Pushed.notify_all();
    }

    void ResetQueue(const int totalWorkerThreads)
    {
        std::lock_guard lock(m_Mutex);
        m_WorkersWaiting = 0;
        m_Suspended = false;
        m_Started = false;
        m_Draining = false;
        m_TotalWorkerThreads = totalWorkerThreads;
        m_Threads.clear();
        m_Threads.reserve(m_TotalWorkerThreads);
    }
};

