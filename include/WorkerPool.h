#pragma once

#include "PriorityTaskQueue.h"
#include "Task.h"

#include <atomic>
#include <thread>
#include <vector>
#include <functional>

namespace tsched {

    class WorkerPool {
        public:
            using TaskRunner = std::function<void(const Task&)>;

            WorkerPool(std::size_t num_workers, PriorityTaskQueue& queue, TaskRunner runner);
            ~WorkerPool();

            WorkerPool(const WorkerPool&) = delete;
            WorkerPool& operator=(const WorkerPool&) = delete;

            void stop();
            std::size_t size() const { return workers_.size(); }

        private:
            void worker_loop();

            PriorityTaskQueue& queue_;
            TaskRunner runner_;
            std::vector<std::thread> workers_;
            std::atomic<bool> stopping_{false};
    };
}