#include "Worker.h"

namespace wc {

    Worker::Worker(std::function<void()> abortCalls) : m_abort(std::move(abortCalls)) {}

    Worker::~Worker() { Stop(); }

    void Worker::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_thread.joinable()) return;
        m_stopping = false;
        m_thread = std::thread([this]() { Loop(); });
    }

    void Worker::Stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_thread.joinable()) return;
            m_stopping = true;
            m_jobs.clear();
        }
        m_wake.notify_all();
        // a call that waits for the server is cut, so the addon is not held up while it unloads
        if (m_abort) m_abort();
        m_thread.join();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_done.clear();
    }

    void Worker::Post(Job job) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) return;
            m_jobs.push_back(std::move(job));
        }
        m_wake.notify_one();
    }

    void Worker::Loop() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [this]() { return m_stopping || !m_jobs.empty(); });
                if (m_stopping) return;
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }
            std::function<void()> then = job();
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) return;
            m_done.push_back(std::move(then));
        }
    }

    void Worker::Drain() {
        std::deque<std::function<void()>> done;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            done.swap(m_done);
        }
        for (auto& f : done) {
            if (f) f();
        }
    }
}
