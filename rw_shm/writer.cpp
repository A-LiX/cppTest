#include "shared_ring_buffer.hpp"
#include "depth.hpp"
#include <chrono>
#include <thread>

int main() {
    SharedRingBufferWriter<Depth> writer("/depth_ring", 128);

    double price = 100.0;
    uint64_t counter = 0;

    while (true) {
        Depth d;
        d.bid_price = price - 0.1;
        d.bid_volume = 10 + (counter % 5);
        d.ask_price = price + 0.1;
        d.ask_volume = 15 + (counter % 3);
        d.ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();

        writer.write(d);
        std::cout << "Wrote Depth: bid=" << d.bid_price
                  << " ask=" << d.ask_price
                  << " time=" << d.ts << std::endl;

        ++counter;
        price += 0.01;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    return 0;
}