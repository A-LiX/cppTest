#include "models.hpp"
#include "lock_free_shared_buffer.hpp"
#include <chrono>
#include <thread>
#include <iostream>

int main()
{
    try
    {
        LockFreeRingBufferWriter<TradeTick> writer("/tick_lock_free_ring", 128);
        TradeTick tick;

        for (uint64_t i = 0;; ++i)
        {
            tick.trade_id = i;
            tick.price = 30000.0 + (i % 100) * 0.01;
            tick.quantity = 0.001 * ((i % 5) + 1);
            tick.is_buyer_maker = (i % 2 == 0);
            tick.ts_exchange = tick.ts_local;
            tick.symbol = "BTCUSDT";
            tick.ts_local = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch())
                                .count();
            writer.write(tick);

            std::this_thread::sleep_for(std::chrono::microseconds(1000)); // 控制写频率
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Writer error: " << e.what() << std::endl;
        return 1;
    }
}