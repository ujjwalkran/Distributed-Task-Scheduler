cmake_minimum_required(VERSION 3.16)
project(distributed_task_scheduler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

find_package(Threads REQUIRED)

add_library(scheduler_core
    src/Scheduler.cpp
    src/WorkerPool.cpp
)
target_include_directories(scheduler_core PUBLIC include)
target_link_libraries(scheduler_core 
    PUBLIC 
        Threads::Threads
        nlohmann_json::nlohmann_json
)

add_executable(task_scheduler src/main.cpp)
target_link_libraries(task_scheduler 
    PRIVATE 
        scheduler_core
        httplib::httplib
        nlohmann_json::nlohmann_json
)

add_executable(scheduler_bench bench/benchmark.cpp)
target_link_libraries(scheduler_bench 
    PRIVATE 
        scheduler_core
)

if(WIN32)
    target_link_libraries(scheduler_core PRIVATE ws2_32)
endif()