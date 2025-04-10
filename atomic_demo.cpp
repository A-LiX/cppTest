#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

std::atomic<int> counter(0); // 原子变量

void increment(int times) {
    for (int i = 0; i < times; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
        // 或者：counter++;
    }
}

int main() {
    const int thread_count = 10;
    const int add_times = 100000;

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(increment, add_times);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final counter value: " << counter << std::endl;
    return 0;
}