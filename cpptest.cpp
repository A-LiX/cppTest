#include <iostream>
#include <string>
#include <chrono>        // 用于计时
#include <simdjson.h>  // 确保 simdjson.h 在当前目录或 include 路径下

// 简化时间点获取
using namespace std::chrono;

int main() {
    // 模拟 binance 返回的 ticker 数据
    std::string json = R"({
        "e": "24hrTicker",
        "E": 1718736000000,
        "s": "BTCUSDT",
        "p": "567.890",
        "P": "3.45%",
        "w": "17000.12",
        "x": "16980.00",
        "c": "17010.20",
        "Q": "0.500",
        "b": "17005.00",
        "B": "0.100",
        "a": "17015.30",
        "A": "0.200"
    })";

    // --- 开始计时 ---
    auto start = high_resolution_clock::now();

    // 初始化 simdjson 对象
    simdjson::padded_string padded_json(json);
    simdjson::dom::parser parser;
    simdjson::dom::element doc;

    // 解析 JSON
    auto error = parser.parse(padded_json).get(doc);
    if (error) {
        std::cerr << "JSON parse error: " << error << std::endl;
        return -1;
    }

    // 提取字段
    std::string_view symbol;
    std::string_view last_price;

    doc["s"].get(symbol);   // symbol
    doc["c"].get(last_price); // last price

    // --- 结束计时 ---
    auto end = high_resolution_clock::now();

    // 计算耗时（纳秒）
    auto duration_ns = duration_cast<nanoseconds>(end - start).count();
    auto duration_us = duration_cast<microseconds>(end - start).count();

    // 输出结果
    std::cout << "Symbol: " << symbol << std::endl;
    std::cout << "Last Price: " << last_price << std::endl;

    std::cout << "解析耗时: " << duration_ns << " ns (" << duration_us << " µs)" << std::endl;

    return 0;
}