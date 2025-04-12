#include <uwebsockets/App.h>
#include <iostream>
#include <simdjson.h>

using namespace uWS;

// 定义 PerSocketData 结构体，用于存储每个 WebSocket 连接的数据
struct PerSocketData {
    // 可以在这里定义需要存储的数据
};

int main() {
    // 初始化 simdjson 解析器
    simdjson::ondemand::parser parser;

    // 使用 uWS::App 替代 Hub
    uWS::App app;

    // 定义 WebSocket 客户端行为
    app.ws<PerSocketData>("/*", {
        .compression = uWS::DISABLED, // 禁用压缩
        .maxPayloadLength = 16 * 1024, // 设置最大负载长度
        .open = [](auto* ws) {
            std::cout << "WebSocket opened!" << std::endl;

            // 发送订阅请求
            const char* subscribeMsg = R"({"method":"SUBSCRIBE","params":["btcusdt@trade"],"id":1})";
            ws->send(subscribeMsg, uWS::OpCode::TEXT);
        },
        .message = [&parser](auto* ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;

            // 使用 simdjson 解析消息
            simdjson::padded_string_view json_view(message.data(), message.size());
            simdjson::ondemand::document doc;

            if (auto error = parser.iterate(json_view).get(doc)) {
                std::cerr << "JSON Parse Error: " << error << std::endl;
                std::cerr << "Raw message: " << message << std::endl;
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

    // 发起 WebSocket 连接
    app.listen(3000, [](auto* listenSocket) {
        if (listenSocket) {
            std::cout << "Listening on port 3000" << std::endl;
        }
    });

    // 连接到 Binance WebSocket
    app.connect("wss://stream.binance.com:9443/ws/btcusdt@trade", nullptr, {}, 5000);

    // 运行事件循环
    std::cout << "Starting event loop..." << std::endl;
    app.run();
    std::cout << "Event loop ended." << std::endl;

    return 0;
}