cmake_minimum_required(VERSION 3.10)
project(ThreadRaceBenchmark)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Если используете Google Benchmark
find_package(benchmark REQUIRED)

add_executable(thread_race main.cpp)
target_link_libraries(thread_race benchmark::benchmark)
