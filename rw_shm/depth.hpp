// depth.hpp
#pragma once

#include <cstdint>

struct Depth {
    double bid_price;
    double bid_volume;
    double ask_price;
    double ask_volume;
    uint64_t ts;  // 时间戳（纳秒）
};