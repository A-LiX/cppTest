#pragma once

#include <cstdint>
#include <string>
#include <array>

// Level 1 Depth（简化版，只保留 top bid / ask）
struct Depth {
    double bid_price;
    double bid_volume;
    double ask_price;
    double ask_volume;
    uint64_t ts;  // 本地或交易所时间戳（纳秒）
};

// Binance 成交 tick（trade event）
struct TradeTick {
    char symbol[7];   // 交易对，如 BTCUSDT
    double price;         // 成交价格
    double quantity;      // 成交数量
    uint64_t trade_id;    // 成交 ID
    uint64_t ts_exchange; // 交易所时间戳（毫秒）
    uint64_t ts_local;    // 本地接收时间（纳秒）
    bool is_buyer_maker;  // 是否为卖出主导（买单被动成交）
};

// Binance L2 Depth Tick（可选 N 档，默认这里只做 top 5 示例）
constexpr int MAX_DEPTH_LEVELS = 5;

struct DepthLevel {
    double price;
    double quantity;
};

struct DepthSnapshot {
    std::string symbol;
    std::array<DepthLevel, MAX_DEPTH_LEVELS> bids;
    std::array<DepthLevel, MAX_DEPTH_LEVELS> asks;
    uint64_t last_update_id;  // Binance depth stream 的 updateId
    uint64_t ts_exchange;     // 交易所时间戳（毫秒）
    uint64_t ts_local;        // 本地接收时间（纳秒）
};