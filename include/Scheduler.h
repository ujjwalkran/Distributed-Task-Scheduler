#pragma once

#include "PriorityTaskQueue.h"
#include "Task.h"
#include "WorkerPool.h"

#include <atomic>
#include <memory>
#include <string>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tsched {

    struct JobRecord {
        Task task;
        JobStatus status = JobStatus::Pending;
        std::chrono::steady_clock::time_point created_at;
        std::optional<std::chrono::steady_clock::time_point> started_at;
        std::optional<std::chrono::steady_clock::time_point> finished_at;
        std::string error;
    };

    class Scheduler {
        public:
            explicit Scheduler(std::size_t num_workers);
            ~Scheduler();

            Scheduler(const Scheduler&) = delete;
            Scheduler& operator=(const Scheduler&) = delete;

            // Submit a Job. Returns the generated Job ID.
            std::string submit(int priority, std::chrono::milliseconds duration, std::string name);

            // Lookup a Job snapshot. Returns nullopt if id is unknown.
            std::optional<JobRecord> get(const std::string& id) const;

            // Mark a Job cancelled. If the Job is pending, it will be skipped by workers.
            // Returns true if cancellation took effect (job was pending or running -> requested).
            bool cancel(const std::string& id);

            //List all Job snapshots(copy).
            std::vector<JobRecord> list() const;

            std::size_t pending_count() const { return queue_.size(); }
            std::size_t worker_count() const { return pool_.size(); }
        
        private:
            void execute(const Task& task);

            mutable std::mutex mu_;
            std::unordered_map<std::string, JobRecord> jobs_;
            std::unordered_set<std::string> cancelled_;

            PriorityTaskQueue queue_;
            WorkerPool pool_;

            std::atomic<std::uint64_t> seq_{0};
    };
}
