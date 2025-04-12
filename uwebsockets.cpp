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
        .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;
        },
        .close = [](auto* ws, int code, std::string_view msg) {
            std::cout << "Disconnected. Code: " << code << std::endl;
        }
    });

    // 发起 WebSocket 连接
    app.connect("wss://stream.binance.com:9443/stream?streams=", [](auto* res, auto* req) {
        std::cout << "WebSocket connection established!" << std::endl;
    });

    // 运行事件循环
    std::cout << "Starting event loop..." << std::endl;
    app.run();
    std::cout << "Event loop ended." << std::endl;

    return 0;
}