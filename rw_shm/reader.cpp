// reader.cpp
#include "depth.hpp"
#include "shared_ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>  

int main() {
    SharedRingBufferReader<Depth> reader("/depth_ring", 128);
    while (true) {
        Depth d;
        if (reader.try_read(d)) {
            std::cout << std::fixed << std::setprecision(2);  // 固定两位小数
            std::cout << "[DepthReader] bid=" << d.bid_price
                      << " vol=" << d.bid_volume
                      << " | ask=" << d.ask_price
                      << " vol=" << d.ask_volume
                      << " @ts=" << d.ts << std::endl;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}