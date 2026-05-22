#include "Scheduler.h"

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace tsched {

    namespace {
        std::string make_id() {
            static thread_local std::mt19937_64 rng{std::random_device{}()};
            std::uniform_int_distribution<std::uint64_t> dist;
            std::ostringstream oss;
            oss << std::hex << dist(rng);
            return oss.str();
        }
    }

    Scheduler::Scheduler(std::size_t num_workers)
        : pool_(num_workers, queue_, [this](const Task& t) { execute(t); }) {}
    
    Scheduler::~Scheduler() {
        pool_.stop();
    }

    std::string Scheduler::submit(int priority, std::chrono::milliseconds duration, std:: string name) {
        Task task;
        task.id = make_id();
        task.priority = priority;
        task.duration = duration;
        task.name = std::move(name);
        task.enqueued_at = std::chrono::steady_clock::now();
        task.seq = seq_.fetch_add(1, std::memory_order_relaxed);

        // capture id before moving `task` into the queue so we can lookup/return it
        std::string id = task.id;
        {
            std::lock_guard<std::mutex> lock(mu_);
            std::cerr << "[debug] Scheduler::submit - preparing JobRecord for id=" << id << std::endl;
            JobRecord rec;
            rec.task = task;
            rec.status = JobStatus::Pending;
            rec.created_at = task.enqueued_at;
            std::cerr << "[debug] Scheduler::submit - before jobs_.emplace for id=" << id << std::endl;
            jobs_.emplace(id, std::move(rec));
            std::cerr << "[debug] Scheduler::submit - after jobs_.emplace for id=" << id << std::endl;
        }

        queue_.push(std::move(task));
        std::cerr << "[debug] Scheduler::submit - after queue_.push" << std::endl;
        std::cerr << "[debug] Scheduler::submit - returning id=" << id << std::endl;
        return id;

    }

    std::optional<JobRecord> Scheduler::get(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = jobs_.find(id);
        if(it == jobs_.end()) return std::nullopt;
        return it->second;
    }

    bool Scheduler::cancel(const std::string& id) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = jobs_.find(id);
        if(it == jobs_.end()) return false;
        if(it->second.status == JobStatus::Completed || 
            it->second.status == JobStatus::Failed ||
            it->second.status == JobStatus::Cancelled) {
                return false;
        }
        cancelled_.insert(id);
        if(it->second.status == JobStatus::Pending) {
            it->second.status = JobStatus::Cancelled;
            it->second.finished_at = std::chrono::steady_clock::now();
        }
        return true;
    }

    std::vector<JobRecord> Scheduler::list() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<JobRecord> out;
        out.reserve(jobs_.size());
        for(const auto& kv : jobs_) out.push_back(kv.second);
        return out;
    }

    void Scheduler::execute(const Task& t) {
        {
            std::lock_guard<std::mutex> lock(mu_);

            if(cancelled_.count(t.id)) {
                auto it = jobs_.find(t.id);

                if(it != jobs_.end() && 
                    it->second.status == JobStatus::Pending) {
                        
                    it->second.status = JobStatus::Cancelled;
                    it->second.finished_at = std::chrono::steady_clock::now();
                }
                return;
            }
            auto it = jobs_.find(t.id);
            if(it != jobs_.end()) {
                it->second.status = JobStatus::Running;
                it->second.started_at = std::chrono::steady_clock::now();
            }
        }

        std::this_thread::sleep_for(t.duration);

        {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = jobs_.find(t.id);
            if(it == jobs_.end()) return;
            if(cancelled_.count(t.id)) {
                it->second.status = JobStatus::Cancelled;
            } else {
                it->second.status = JobStatus::Completed;
            }
            it->second.finished_at = std::chrono::steady_clock::now();
        }
    }
}

