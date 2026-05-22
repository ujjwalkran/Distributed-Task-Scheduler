#pragma once

#include "Task.h"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>
#include <vector>

namespace tsched
{
    // Thread-safe priority queue. Producers push tasks; workers block on pop() until a task is available
    // or the queue is stopped.
    class PriorityTaskQueue
    {
    public:
        void push(Task task)
        {
            {
                std::lock_guard<std::mutex> lock(mu_);
                heap_.push(std::move(task));
            }
            cv_.notify_one();
        }

        // Block util a tasj is availabe or shutdown() is called.
        // Returns nullopt if shutdown() was called and the queue is empty.
        std::optional<Task> pop()
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [&]
                    { return shutdown_ || !heap_.empty(); });
            if (heap_.empty())
            {
                return std::nullopt;
            }
            Task task = heap_.top();
            heap_.pop();
            return task;
        }

        void shutdown()
        {
            {
                std::lock_guard<std::mutex> lock(mu_);
                shutdown_ = true;
            }
            cv_.notify_all();
        }

        std::size_t size() const
        {
            std::lock_guard<std::mutex> lock(mu_);
            return heap_.size();
        }

    private:
        mutable std::mutex mu_;
        std::condition_variable cv_;
        std::priority_queue<Task, std::vector<Task>, TaskGreater> heap_;
        bool shutdown_ = false;
    };

    //Plain FIFO queue used only by the benchmark as the naive baseline
    class FifoTaskQueue {
        public:
            void push(Task task) {
                {
                    std::lock_guard<std::mutex> lock(mu_);
                    q_.push(std::move(task));
                }
                cv_.notify_one();
            }

            std::optional<Task> pop() {
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait(lock, [&] { return shutdown_ || !q_.empty(); });
                if (q_.empty()) {
                    return std::nullopt;
                }
                Task task = std::move(q_.front());
                q_.pop();
                return task;
            }

            void shutdown() {
                {
                    std::lock_guard<std::mutex> lock(mu_);
                    shutdown_ = true;
                }
                cv_.notify_all();
            }

        private:
            mutable std::mutex mu_;
            std::condition_variable cv_;
            std::queue<Task> q_;
            bool shutdown_ = false;
    };
} // namespace tsched