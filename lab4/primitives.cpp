#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <random>
#include <chrono>
#include <semaphore>
#include <barrier>
#include <iomanip>
#include <functional>
#include <algorithm>

using namespace std;

constexpr int THREAD_COUNT = 8;
constexpr int ITERATIONS = 100000; 
constexpr int BARRIER_ITERATIONS = 1000;
constexpr int ASCII_START = 32; 
constexpr int ASCII_END = 126;

vector<char> shared_buffer;
mutex buffer_mutex;

char get_random_char() {
    thread_local random_device rd;
    thread_local mt19937 gen(rd());
    thread_local uniform_int_distribution<> dis(ASCII_START, ASCII_END);
    return static_cast<char>(dis(gen));
}

//Мьютекс
void mutex_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        lock_guard<mutex> lock(buffer_mutex);
        shared_buffer.push_back(get_random_char());
    }
}

//Семафор
counting_semaphore<> sem(1);
void semaphore_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        sem.acquire();
        shared_buffer.push_back(get_random_char());
        sem.release();
    }
}

//Спинлок
atomic_flag spinlock = ATOMIC_FLAG_INIT;
void spinlock_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        while (spinlock.test_and_set(memory_order_acquire)) {
            std::this_thread::yield(); 
        }
        shared_buffer.push_back(get_random_char());
        spinlock.clear(memory_order_release);
    }
}

//Спинвейт
atomic<bool> busy_flag{false};
void spinwait_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        while (busy_flag.exchange(true, memory_order_acquire)) {
             std::this_thread::yield();
        }
        shared_buffer.push_back(get_random_char());
        busy_flag.store(false, memory_order_release);
    }
}

//Монитор
mutex monitor_mtx;
condition_variable monitor_cv;
bool resource_free = true;

void monitor_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        unique_lock<mutex> lock(monitor_mtx);
        monitor_cv.wait(lock, [] { return resource_free; });
        
        resource_free = false;
        lock.unlock(); 
        
        shared_buffer.push_back(get_random_char());
        
        lock.lock();
        resource_free = true;
        monitor_cv.notify_one(); // (аналог pulse/exit)
    }
}

//Барьер
barrier<> sync_barrier(THREAD_COUNT);
mutex barrier_mutex_internal;

void barrier_worker(int iters) {
    for (int i = 0; i < iters; ++i) {
        {
            lock_guard<mutex> lock(barrier_mutex_internal);
            shared_buffer.push_back(get_random_char());
        }
        sync_barrier.arrive_and_wait(); 
    }
}

template<typename Func>
double run_and_measure(Func func, const string& name, int iter_count) {
    shared_buffer.clear();
    shared_buffer.reserve(THREAD_COUNT * iter_count);
    
    vector<thread> threads;
    threads.reserve(THREAD_COUNT);
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < THREAD_COUNT; ++i)
        threads.emplace_back(func, iter_count);
    
    for (auto& t : threads)
        t.join();
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << left << setw(12) << name 
         << " | Time: " << setw(6) << duration.count() << " ms"
         << " | Count: " << shared_buffer.size() << endl;
    
    return duration.count();
}

int main() {
    cout << "Анализ примитивов синхронизации (потоки: " << THREAD_COUNT << ") ===\n\n";
    
    vector<pair<string, double>> results;
    
    results.emplace_back("SpinLock", run_and_measure(spinlock_worker, "SpinLock", ITERATIONS));
    results.emplace_back("SpinWait", run_and_measure(spinwait_worker, "SpinWait", ITERATIONS));
    results.emplace_back("Mutex", run_and_measure(mutex_worker, "Mutex", ITERATIONS));
    results.emplace_back("Semaphore", run_and_measure(semaphore_worker, "Semaphore", ITERATIONS));
    results.emplace_back("Monitor", run_and_measure(monitor_worker, "Monitor", ITERATIONS));
    
    cout << "Примечание: Метод барьера выполняет меньше итераций (" << BARRIER_ITERATIONS << ") из-за накладных расходов.\n";
    double barrier_time = run_and_measure(barrier_worker, "Barrier", BARRIER_ITERATIONS);
    double projected_barrier = barrier_time * (static_cast<double>(ITERATIONS) / BARRIER_ITERATIONS);
    results.emplace_back("Barrier (est)", projected_barrier);

    cout << "\nСравнительные результаты (отсортированные по скорости)\n";
    sort(results.begin(), results.end(), 
         [](const auto& a, const auto& b) { return a.second < b.second; });
    
    for (const auto& [name, time] : results) {
        cout << left << setw(15) << name << ": ~" << static_cast<long>(time) << " ms";
        if (name == results[0].first) cout << " (Winner)";
        cout << "\n";
    }

    return 0;
}
