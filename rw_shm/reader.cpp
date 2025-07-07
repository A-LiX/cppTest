#include "models.hpp"
#include "lock_free_shared_buffer.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    try {
        LockFreeRingBufferReader<TradeTick> reader("/tick_lock_free_ring", 128);
        TradeTick tick;

        while (true) {
            if (reader.try_read(tick)) {
                // 当前时间
                auto now = std::chrono::high_resolution_clock::now();
                uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      now.time_since_epoch())
                                      .count();

                // 延迟分析
                uint64_t delay = now_ns - tick.ts_local;

                // 打印结果
                std::cout 
                          << " | delay(ns): " << delay
                          << std::endl;
            } else {
                // 可选：避免过度空转
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Reader error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}