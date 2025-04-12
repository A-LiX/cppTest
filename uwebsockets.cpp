#include <uwebsockets/App.h>
#include <iostream>
#include <simdjson.h>

using namespace uWS;

int main() {
    struct PerSocketData {}; // 保留会话数据占位

    // 初始化 simdjson 解析器
    simdjson::ondemand::parser parser;

    // 使用 uWS::App 替代直接操作 Loop
    uWS::App app = uWS::App()
        .ws<PerSocketData>("/*", { // 配置 WebSocket 行为模板
            .compression = DISABLED,
            .maxPayloadLength = 16 * 1024,
            .open = [](auto* ws) {
                std::cout << "Connected to Binance!" << std::endl;

                // 发送订阅请求
                const char* subscribeMsg = R"({"method":"SUBSCRIBE","params":["btcusdt@trade"],"id":1})";
                ws->send(subscribeMsg, OpCode::TEXT);
            },
            .message = [&parser](auto* ws, std::string_view message, OpCode opCode) {
                // 使用 simdjson 解析消息
                simdjson::padded_string_view json_view(message.data(), message.size());
                simdjson::ondemand::document doc;

                if (auto error = parser.iterate(json_view).get(doc)) {
                    std::cerr << "JSON Parse Error: " << error << std::endl;
                    return;
                }

                simdjson::ondemand::value price, quantity;
                if (!doc["p"].get(price) && !doc["q"].get(quantity)) {
                    std::cout << "BTC/USDT Price: " << price.get_string().value() 
                              << ", Quantity: " << quantity.get_string().value() 
                              << std::endl;
                } else {
                    std::cerr << "Invalid trade data format" << std::endl;
                }
            },
            .close = [](auto* ws, int code, std::string_view msg) {
                std::cout << "Disconnected. Code: " << code << std::endl;
            }
        });

    // 连接到 Binance WebSocket（客户端模式）
    app.connect("wss://stream.binance.com:9443/ws/btcusdt@trade", nullptr, {}, 5000);

    // 运行事件循环
    app.run();
}