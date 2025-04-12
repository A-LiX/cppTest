#include <uWebSockets/App.h>
#include <iostream>
#include <simdjson.h>

using namespace uWS;

int main() {
    // 初始化 simdjson 解析器
    simdjson::ondemand::parser parser;

    // 使用 Hub 管理事件循环
    uWS::Hub hub;

    // 定义 WebSocket 客户端行为
    hub.onConnection([&hub](uWS::WebSocket<CLIENT>* ws, uWS::HttpRequest req) {
        std::cout << "Connected to Binance!" << std::endl;

        // 发送订阅请求
        const char* subscribeMsg = R"({"method":"SUBSCRIBE","params":["btcusdt@trade"],"id":1})";
        ws->send(subscribeMsg, OpCode::TEXT);
    });

    hub.onMessage([&parser](uWS::WebSocket<CLIENT>* ws, 
                           std::string_view message, 
                           OpCode opCode) {
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
    });

    hub.onDisconnection([](uWS::WebSocket<CLIENT>* ws, 
                       int code, 
                       std::string_view msg) {
        std::cout << "Disconnected. Code: " << code << std::endl;
    });

    // 发起 WebSocket 连接
    hub.connect("wss://stream.binance.com:9443/ws/btcusdt@trade", nullptr);

    // 运行事件循环
    hub.run();
}