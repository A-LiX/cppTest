#include <uWebSockets/App.h>
#include <iostream>
#include <simdjson.h>  // 使用 simdjson 替代 jsoncpp

using namespace uWS;

int main() {
    struct PerSocketData { /* 会话数据（可选） */ };

    // 初始化 simdjson 解析器（线程安全）
    simdjson::ondemand::parser parser;
    uWS::Loop* loop = uWS::Loop::get();

    WebSocket<false, true, PerSocketData>* ws = nullptr;

    WebSocketBehavior behavior{
        .compression = uWS::DISABLED,
        .maxPayloadLength = 16 * 1024,
        .open = [&ws](WebSocket<false, true, PerSocketData>* webSocket) {
            std::cout << "Connected to Binance!" << std::endl;
            ws = webSocket;

            // 订阅 BTC/USDT 交易流
            const char* subscribeMsg = R"({"method":"SUBSCRIBE","params":["btcusdt@trade"],"id":1})";
            webSocket->send(subscribeMsg, uWS::OpCode::TEXT);
        },
        .message = [&parser](WebSocket<false, true, PerSocketData>* webSocket, 
                            std::string_view message, 
                            uWS::OpCode opCode) {
            // 使用 simdjson 解析消息
            simdjson::padded_string_view json_view(message.data(), message.size());
            simdjson::ondemand::document doc;

            // 尝试解析 JSON
            if (auto error = parser.iterate(json_view).get(doc)) {
                std::cerr << "JSON Parse Error: " << error << std::endl;
                return;
            }

            // 提取交易价格和数量（直接访问字段）
            simdjson::ondemand::value price, quantity;
            if (!doc["p"].get(price) && !doc["q"].get(quantity)) {
                std::cout << "BTC/USDT Price: " << price.get_string().value() 
                          << ", Quantity: " << quantity.get_string().value() 
                          << std::endl;
            } else {
                std::cerr << "Invalid trade data format" << std::endl;
            }
        },
        .close = [](WebSocket<false, true, PerSocketData>* webSocket, int code, std::string_view msg) {
            std::cout << "Disconnected. Code: " << code << std::endl;
        }
    };

    // 连接到 Binance WebSocket
    loop->connect<PerSocketData>("wss://stream.binance.com:9443/ws/btcusdt@trade", behavior, {});
    loop->run();

    return 0;
}