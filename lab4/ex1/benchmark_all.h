#pragma once

#include "../benchmark/include/benchmark/benchmark.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <semaphore>
#include <barrier>
#include <vector>
#include <chrono>
#include <random>

class CompleteSyncBenchmark {
private:
    static char generateRandomChar() {
        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(33, 126);
        return static_cast<char>(distribution(generator));
    }
    
    static void simulatedWork() {
        volatile char c = generateRandomChar();
        (void)c;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
public:
    static void BM_Mutex(benchmark::State& state) {
        std::mutex mtx;
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&mtx, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        std::lock_guard<std::mutex> lock(mtx);
                        simulatedWork();
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
    
    static void BM_Semaphore(benchmark::State& state) {
        std::counting_semaphore<1000> sem(1);
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&sem, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        sem.acquire();
                        simulatedWork();
                        sem.release();
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
    
    static void BM_Barrier(benchmark::State& state) {
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            std::barrier sync_point(num_threads);
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&sync_point, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        simulatedWork();
                        sync_point.arrive_and_wait();
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
    
    static void BM_SpinLock(benchmark::State& state) {
        std::atomic_flag lock = ATOMIC_FLAG_INIT;
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&lock, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        while (lock.test_and_set(std::memory_order_acquire)) {}
                        simulatedWork();
                        lock.clear(std::memory_order_release);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
    
    static void BM_SpinWait(benchmark::State& state) {
        std::atomic<bool> lock(false);
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&lock, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        bool expected = false;
                        while (!lock.compare_exchange_weak(expected, true, 
                                std::memory_order_acquire, std::memory_order_relaxed)) {
                            expected = false;
                            if (j % 10 == 0) {
                                std::this_thread::yield();
                            }
                        }
                        simulatedWork();
                        lock.store(false, std::memory_order_release);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
    
    static void BM_Monitor(benchmark::State& state) {
        std::mutex mtx;
        std::condition_variable cv;
        bool available = true;
        std::vector<std::thread> threads;
        int num_threads = state.range(0);
        int iterations = state.range(1);
        
        for (auto _ : state) {
            threads.clear();
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&mtx, &cv, &available, iterations]() {
                    for (int j = 0; j < iterations; ++j) {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [&available]() { return available; });
                        available = false;
                        
                        simulatedWork();
                        
                        available = true;
                        lock.unlock();
                        cv.notify_one();
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
    }
};

BENCHMARK(CompleteSyncBenchmark::BM_Mutex)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);

BENCHMARK(CompleteSyncBenchmark::BM_Semaphore)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);

BENCHMARK(CompleteSyncBenchmark::BM_Barrier)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);

BENCHMARK(CompleteSyncBenchmark::BM_SpinLock)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);

BENCHMARK(CompleteSyncBenchmark::BM_SpinWait)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);

BENCHMARK(CompleteSyncBenchmark::BM_Monitor)
    ->Args({4, 50})->Args({8, 50})->Args({16, 50})->Unit(benchmark::kMicrosecond);
