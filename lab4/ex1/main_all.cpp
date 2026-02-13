#include "benchmark_all.h"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "   GOOGLE BENCHMARK - ALL 6 SYNCHRONIZATION PRIMITIVES" << std::endl;
    std::cout << "Testing: Mutex, Semaphore, Barrier, SpinLock, SpinWait, Monitor" << std::endl;
    std::cout << "Threads: 4, 8, 16 | Iterations: 50" << std::endl;
    
    ::benchmark::Initialize(&argc, argv);
    
    if (argc == 1) {
        ::benchmark::RunSpecifiedBenchmarks();
    } else {
        ::benchmark::RunSpecifiedBenchmarks();
    }
    
    std::cout << "   Benchmark completed successfully!" << std::endl;
    
    return 0;
}
