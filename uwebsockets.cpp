#include <uwebsockets/App.h>
#include <simdjson.h>
#include <iostream>

int main() {
    struct PerSocketData {};

    simdjson::ondemand::parser parser;

    // 使用新版 uWebSockets 连接方式
    uWS::App().ws<PerSocketData>("/ws", {
        // 连接时触发的事件
        .open = [](auto *ws) {
            std::cout << "✅ Connected to Binance WebSocket\n";
        },
        // 消息到来时触发的事件
        .message = [&parser](auto *ws, std::string_view message, uWS::OpCode) {
            try {
                simdjson::padded_string padded(message);
                auto doc = parser.iterate(padded);

                std::string_view price = doc["p"].get_string().value();
                std::string_view qty   = doc["q"].get_string().value();

                std::cout << "📈 Price: " << price << " | Qty: " << qty << "\n";
            } catch (const simdjson::simdjson_error &e) {
                std::cerr << "❌ simdjson parse error: " << e.what() << "\n";
            }
        },
        // 连接关闭时触发的事件
        .close = [](auto *ws, int code, std::string_view msg) {
            std::cout << "🔌 Disconnected: " << code << ", " << msg << "\n";
        }
    }).connect("wss://stream.binance.com:9443/ws/btcusdt@trade", nullptr);

    // 运行事件循环
    uWS::Loop::get()->run();
    return 0;
}