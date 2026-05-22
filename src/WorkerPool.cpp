#include "WorkerPool.h"

namespace tsched {

    WorkerPool::WorkerPool(std::size_t num_workers, PriorityTaskQueue& queue, TaskRunner runner)
        : queue_(queue), runner_(std::move(runner)) {
            workers_.reserve(num_workers);
            for(std::size_t i = 0; i < num_workers; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            } 
    }
    
    WorkerPool::~WorkerPool() {
        stop();
    }

    void WorkerPool::stop() {
        if (stopping_.exchange(true)) return;
        queue_.shutdown();
        for(auto& t : workers_) {
            if(t.joinable()) t.join();
        }
    }

    void WorkerPool::worker_loop() {
        while (!stopping_.load(std::memory_order_acquire)) {
            auto task = queue_.pop();
            if(!task) return;
            runner_(*task);
        }
    }
}