#pragma once

#include <chrono>
#include <functional>
#include <string>

namespace tsched {
    enum class JobStatus {
        Pending,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    inline const char* to_string(JobStatus status) {
        switch (status) {
            case JobStatus::Pending: return "Pending";
            case JobStatus::Running: return "Running";
            case JobStatus::Completed: return "Completed";
            case JobStatus::Failed: return "Failed";
            case JobStatus::Cancelled: return "Cancelled";
        }
        return "Unknown";
    }

    struct Task {
        std::string id;
        int priority = 0;    // Higher value means higher priority
        std::chrono::milliseconds duration;   //simulated execution time
        std::string name;
        std::chrono::steady_clock::time_point enqueued_at;
        std::uint64_t seq = 0;  //tie-breaker for FIFO within same priority
    };

    //Min-heap by (-priority, seq) so highest priority pops first, FIFO within ties.
    struct TaskGreater {
        bool operator()(const Task& a, const Task& b) const {
            if (a.priority != b.priority) {
                return a.priority < b.priority; // higher priority first
            }
            return a.seq > b.seq; // earlier sequence first
        }
    };
}