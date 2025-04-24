#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include "lock_free_ring_buffer.hpp"  // 替换为你的头文件名或内联定义

constexpr size_t kCapacity = 1024;
constexpr size_t kProducerCount = 4;
constexpr size_t kConsumerCount = 4;
constexpr size_t kOperationsPerProducer = 100000;

int main() {
    LockFreeRingBuffer<int, kCapacity> queue;
    std::atomic<size_t> totalConsumed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 启动生产者线程
    for (size_t i = 0; i < kProducerCount; ++i) {
        producers.emplace_back([&queue]() {
            for (size_t j = 0; j < kOperationsPerProducer; ++j) {
                while (!queue.enqueue(j)) {
                    std::this_thread::yield();  // 避免忙等
                }
            }
        });
    }

    // 启动消费者线程
    for (size_t i = 0; i < kConsumerCount; ++i) {
        consumers.emplace_back([&queue, &totalConsumed]() {
            int value;
            while (true) {
                if (totalConsumed.load() >= kProducerCount * kOperationsPerProducer) break;

                if (queue.dequeue(value)) {
                    totalConsumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "All threads done.\n";
    std::cout << "Total consumed: " << totalConsumed.load() << "\n";
    std::cout << "Time taken: " << ms << " ms\n";

    return 0;
}