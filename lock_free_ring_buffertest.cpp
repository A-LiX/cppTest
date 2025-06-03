#include "lock_free_ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

constexpr size_t CAP = 1024;
constexpr int N = 10000;

void test_mpmc() {
    LockFreeMPMCRingBuffer<int, CAP> q;
    std::atomic<int> sum{0};
    std::thread prod([&]() {
        for (int i = 1; i <= N; ++i) {
            while (!q.enqueue(i));
        }
    });
    std::thread cons([&]() {
        int cnt = 0, v;
        while (cnt < N) {
            if (q.dequeue(v)) {
                sum += v;
                ++cnt;
            }
        }
    });
    prod.join();
    cons.join();
    std::cout << "MPMC sum: " << sum << std::endl;
}

void test_mpsc() {
    LockFreeMPSCQueue<int, CAP> q;
    std::atomic<int> sum{0};
    std::thread prod1([&]() { for (int i = 1; i <= N/2; ++i) while (!q.enqueue(i)); });
    std::thread prod2([&]() { for (int i = N/2+1; i <= N; ++i) while (!q.enqueue(i)); });
    std::thread cons([&]() {
        int cnt = 0, v;
        while (cnt < N) {
            if (q.dequeue(v)) {
                sum += v;
                ++cnt;
            }
        }
    });
    prod1.join();
    prod2.join();
    cons.join();
    std::cout << "MPSC sum: " << sum << std::endl;
}

void test_spsc() {
    LockFreeSPSCQueue<int, CAP> q;
    int sum = 0;
    std::thread prod([&]() { for (int i = 1; i <= N; ++i) while (!q.enqueue(i)); });
    std::thread cons([&]() {
        int cnt = 0, v;
        while (cnt < N) {
            if (q.dequeue(v)) {
                sum += v;
                ++cnt;
            }
        }
    });
    prod.join();
    cons.join();
    std::cout << "SPSC sum: " << sum << std::endl;
}

int main() {
    test_mpmc();
    test_mpsc();
    test_spsc();
    return 0;
}