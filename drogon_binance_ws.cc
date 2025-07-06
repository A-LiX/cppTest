#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <iostream>
#include <chrono>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // Binance 行情 WebSocket
    std::string binance_server = "wss://stream.binance.com:9443";
    std::string binance_path = "/ws/btcusdt@trade";

    // 创建 Binance WebSocket 客户端
    auto binanceWs = WebSocketClient::newWebSocketClient(binance_server);
    auto binanceReq = HttpRequest::newHttpRequest();
    binanceReq->setPath(binance_path);

    // Binance 消息处理
    binanceWs->setMessageHandler([](const std::string &message,
                                    const WebSocketClientPtr &,
                                    const WebSocketMessageType &type) {
        std::string messageType = "Unknown";
        if (type == WebSocketMessageType::Text)
            messageType = "text";
        else if (type == WebSocketMessageType::Pong)
            messageType = "pong";
        else if (type == WebSocketMessageType::Ping)
            messageType = "ping";
        else if (type == WebSocketMessageType::Binary)
            messageType = "binary";
        else if (type == WebSocketMessageType::Close)
            messageType = "Close";

        auto now = std::chrono::system_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

        LOG_INFO << "[Binance] (" << messageType << ", local_ns=" << ns << "): " << message;
    });

    binanceWs->setConnectionClosedHandler([](const WebSocketClientPtr &) {
        LOG_INFO << "[Binance] WebSocket connection closed!";
    });

    LOG_INFO << "Connecting to Binance WebSocket at " << binance_server << binance_path;
    binanceWs->connectToServer(
        binanceReq,
        [](ReqResult r,
           const HttpResponsePtr &,
           const WebSocketClientPtr &wsPtr) {
            if (r != ReqResult::Ok)
            {
                LOG_ERROR << "[Binance] Failed to establish WebSocket connection!";
                wsPtr->stop();
                return;
            }
            LOG_INFO << "[Binance] WebSocket connected!";
        });

    // 可选：15秒后自动退出
    // app().getLoop()->runAfter(15, []() { app().quit(); });

    app().setThreadNum(2);
    app().setLogLevel(trantor::Logger::kDebug);
    app().run();
    LOG_INFO << "bye!";
    return 0;
}

// g++ drogon_binance_ws.cc -o ws_binance -std=c++20 \
//   -I/opt/homebrew/include \
//   -L/opt/homebrew/lib \
//   -ldrogon -ltrantor -ljsoncpp \
//   -lssl -lcrypto \
//   -lbrotlidec -lbrotlienc -lbrotlicommon \
//   -lz -lcares -lhiredis -lsqlite3 \
//   -lpthread
