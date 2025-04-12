#include <uwebsockets/App.h>
#include <simdjson.h>
#include <iostream>

int main() {
    struct PerSocketData {};

    simdjson::ondemand::parser parser;

    uWS::App().connect("wss://stream.binance.com:9443/ws/btcusdt@trade", nullptr, {}, 
        [](auto *ws, uWS::WebSocketState<false, true, PerSocketData> *) {
            std::cout << "✅ Connected to Binance WebSocket\n";
        },
        [&parser](auto *ws, std::string_view message, uWS::OpCode) {
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
        nullptr, nullptr,
        [](auto *ws, int code, std::string_view msg) {
            std::cout << "🔌 Disconnected: " << code << ", " << msg << "\n";
        }
    );

    uWS::Loop::get()->run();
    return 0;
}