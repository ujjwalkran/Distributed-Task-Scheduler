#include "PriorityTaskQueue.h"
#include "Task.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <cstdio>
#include <iomanip>

using namespace tsched;
using clock_type = std::chrono::steady_clock;

struct Workload {
    std::vector<Task> tasks;
};

Workload make_workload(std::size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> prio_dist(1, 10);
    std::uniform_int_distribution<int> dur_dist(10, 100);
    Workload w;
    w.tasks.reserve(n);
    for(std::size_t i = 0; i < n; ++i) {
        Task t;
        t.id = "task_" + std::to_string(i);
        t.priority = prio_dist(rng);
        t.duration = std::chrono::milliseconds(dur_dist(rng));
        t.seq = i;
        t.name = "Task " + std::to_string(i);
        w.tasks.push_back(std::move(t));
    }
    return w;
}

template <typename Queue>
long long run(const Workload& w, std::size_t num_workers) {
    Queue q;
    std::atomic<long long> weighted_done{0};
    std::atomic<std::size_t> done_count{0};

    std::vector<std::thread> workers;
    workers.reserve(num_workers);
    for(std::size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back([&]() {
            while(auto t = q.pop()) {
                std::this_thread::sleep_for(t->duration);
                weighted_done.fetch_add(t->priority, std::memory_order_relaxed);
                done_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    auto start = clock_type::now();
    for(const auto& t : w.tasks) {
        q.push(t);
    }
    
    const std::size_t half = w.tasks.size() / 2;
    while(done_count.load(std::memory_order_relaxed) < half) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    long long weighted_at_half = weighted_done.load(std::memory_order_relaxed);
    auto t_half = clock_type::now();

    q.shutdown();
    for(auto& t : workers) {
        t.join();
    }

    auto half_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_half - start).count();
    (void)half_ms; // silence unused variable warning
    return weighted_at_half;
}

int main() {
    std::size_t num_tasks = 10000;
    std::size_t num_workers = 4;
    const unsigned seed = 42;

    auto workload = make_workload(num_tasks, seed);
    auto t0 = clock_type::now();
    long long fifo_w = run<FifoTaskQueue>(workload, num_workers);
    auto t1 = clock_type::now();
    long long prio_w = run<PriorityTaskQueue>(workload, num_workers);
    auto t2 = clock_type::now();
    auto fifo_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto prio_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    double pct = 100.0 * (double(prio_w) - double(fifo_w)) / double(fifo_w);

    std::cout << "Benchmark :" << num_tasks << " mixed-priority tasks: " << num_workers << " workers\n";
    std::cout << " FIFO   weighted done at half completion: " << fifo_w << 
                 " (total run : " << fifo_ms << " ms)\n";
    std::cout << "Priority weighted done at half completion: " << prio_w << 
                 " (total run : " << prio_ms << " ms)\n";
    std::cout << "Improvement: " << std::fixed << std::setprecision(1) << pct << "%\n";
    return 0;
}