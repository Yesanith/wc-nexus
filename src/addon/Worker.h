#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "core/Api.h"

namespace wc {

    /// does the calls to the server one after another on a thread of its own, so that the game never waits for the server, and hands what came
    /// back to the thread that draws, which runs it at its next frame
    class Worker {
    public:
        explicit Worker(std::function<void()> abortCalls = nullptr);
        ~Worker();

        void Start();

        /// no more calls are started; the one under way is cut short and its answer dropped
        void Stop();

        /// [work] runs on the thread of the worker; [ok] with its result, or [fail] with what went wrong, runs on the thread that draws
        template <typename R>
        void Run(std::function<R()> work, std::function<void(R)> ok, std::function<void(const ApiError&)> fail) {
            Post([work, ok, fail]() -> std::function<void()> {
                try {
                    auto result = std::make_shared<R>(work());
                    return [ok, result]() {
                        if (ok) ok(std::move(*result));
                    };
                } catch (const ApiError& e) {
                    return [fail, e]() {
                        if (fail) fail(e);
                    };
                } catch (const std::exception& e) {
                    ApiError error("unknown", e.what(), 0);
                    return [fail, error]() {
                        if (fail) fail(error);
                    };
                }
            });
        }

        /// a call that gives nothing back
        void Do(std::function<void()> work, std::function<void()> ok, std::function<void(const ApiError&)> fail) {
            Run<bool>([work]() { work(); return true; }, [ok](bool) { if (ok) ok(); }, fail);
        }

        /// runs what came back, on the thread that draws. called once a frame
        void Drain();

    private:
        using Job = std::function<std::function<void()>()>;
        void Post(Job job);
        void Loop();

        std::function<void()> m_abort;
        std::mutex m_mutex;
        std::condition_variable m_wake;
        std::deque<Job> m_jobs;
        std::deque<std::function<void()>> m_done;
        std::thread m_thread;
        bool m_stopping = false;
    };
}
