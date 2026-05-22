#include "Scheduler.h"
#include <nlohmann/json.hpp>
#include <httplib.h>


#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

using json = nlohmann::json;
using namespace tsched;

namespace {
    json job_to_json(const JobRecord& r) {
        auto ms = [](const std::chrono::steady_clock::time_point& tp) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
        };
        json j = {
            {"id", r.task.id},
            {"name", r.task.name},
            {"priority", r.task.priority},
            {"duration_ms", r.task.duration.count()},
            {"status", to_string(r.status)},
            {"created_at_ms", ms(r.created_at)},
        };
        if(r.started_at) {
            j["started_at_ms"] = ms(*r.started_at);
        }
        if(r.finished_at) {
            j["finished_at_ms"] = ms(*r.finished_at);
        }
        if(!r.error.empty()) {
            j["error"] = r.error;
        }
        return j;
    }

    httplib::Server* g_server = nullptr;
    void handle_signal(int) {
        if(g_server) g_server->stop();
    }
}

int main(int argc, char** argv) {
    std::size_t workers = std::max(2u, std::thread::hardware_concurrency());
    std::string host = "0.0.0.0";
    int port = 8080;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--workers" && i + 1 < argc) {
            workers = std::stoul(argv[++i]);
        } else if(arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if(arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else {
            std::cerr << "Usage: " << argv[0] << " [--workers N] [--host HOST] [--port PORT]\n";
            return 1;
        }
    }

    Scheduler Scheduler(workers);
    httplib::Server server;
    g_server = &server;
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    server.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"status", "ok"},
            {"pending_jobs", Scheduler.pending_count()},
            {"worker_count", Scheduler.worker_count()}
        };
        res.set_content(j.dump(), "application/json");
    });

    //POST /jobs with JSON body {"name": "Job Name", "priority": 5, "duration_ms": 1000}
    server.Post("/jobs", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::cerr << "[debug] POST /jobs parsed body: " << body.dump() << std::endl;
            int priority = body.value("priority", 0);
            int duration_ms = body.value("duration_ms", 0);
            std::string name = body.value("name", "");
            std::cerr << "[debug] POST /jobs fields -> name='" << name << "' priority=" << priority << " duration_ms=" << duration_ms << std::endl;
            if(duration_ms < 0) {
                throw std::runtime_error("duration_ms must be >= 0");
            }

            std::cerr << "[debug] POST /jobs calling Scheduler.submit" << std::endl;
            auto id = Scheduler.submit(priority, std::chrono::milliseconds(duration_ms), std::move(name));
            std::cerr << "[debug] POST /jobs Scheduler.submit returned id=" << id << std::endl;
            json resp = {{"id", id}, {"status", "pending"}};
            res.status = 201;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {

            res.status = 400;
            json resp = {{"error", e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    server.Get(R"(/jobs/([0-9a-fA-F]+))", [&](const httplib::Request& req, httplib::Response& res) {
        auto id = req.matches[1].str();
        auto rec = Scheduler.get(id);
        if(!rec) {
            res.status = 404;
            res.set_content(json{{"error", "job not found"}}.dump(), "application/json");
            return;
        }
        res.set_content(job_to_json(*rec).dump(), "application/json");
    });

    server.Delete(R"(/jobs/([0-9a-fA-F]+))", [&](const httplib::Request& req, httplib::Response& res) {
        auto id = req.matches[1].str();
        bool cancelled = Scheduler.cancel(id);
        if(!cancelled) {
            res.status = 404;
            res.set_content(json{{"error", "job not found or already completed"}}.dump(), "application/json");
            return;
        }
        res.set_content(json{{"id", id}, {"status", "cancelled"}}.dump(), "application/json");
    });

    server.Get("/jobs", [&](const httplib::Request&, httplib::Response& res) {
        auto jobs = Scheduler.list();
        json arr = json::array();
        for(const auto& r : jobs) {
            arr.push_back(job_to_json(r));
        }
        res.set_content(arr.dump(), "application/json");
    });

    std::cout << "task_scheduler is running on " << host << ":" << port << " with " << workers << " workers.\n";
    if(!server.listen(host.c_str(), port)) {
        std::cerr << "Failed to start server on " << host << ":" << port << "\n";
        return 1;
    }
    return 0;
}