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
    std::string binance_path = "/stream?streams=btcusdt@trade";

    // Crypto.com 行情 WebSocket
    std::string crypto_server = "wss://stream.crypto.com";
    std::string crypto_path = "/v2/market";

    // 创建 Binance WebSocket 客户端
    auto binanceWs = WebSocketClient::newWebSocketClient(binance_server);
    auto binanceReq = HttpRequest::newHttpRequest();
    binanceReq->setPath(binance_path);

    // 创建 Crypto.com WebSocket 客户端
    auto cryptoWs = WebSocketClient::newWebSocketClient(crypto_server);
    auto cryptoReq = HttpRequest::newHttpRequest();
    cryptoReq->setPath(crypto_path);

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

    // Crypto.com 消息处理
    cryptoWs->setMessageHandler([](const std::string &message,
                                   const WebSocketClientPtr &ws,
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

        LOG_INFO << "[Crypto.com] (" << messageType << ", local_ns=" << ns << "): " << message;
    });

    cryptoWs->setConnectionClosedHandler([](const WebSocketClientPtr &) {
        LOG_INFO << "[Crypto.com] WebSocket connection closed!";
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

    LOG_INFO << "Connecting to Crypto.com WebSocket at " << crypto_server << crypto_path;
    cryptoWs->connectToServer(
        cryptoReq,
        [cryptoWs](ReqResult r,
           const HttpResponsePtr &,
           const WebSocketClientPtr &wsPtr) {
            if (r != ReqResult::Ok)
            {
                LOG_ERROR << "[Crypto.com] Failed to establish WebSocket connection!";
                wsPtr->stop();
                return;
            }
            LOG_INFO << "[Crypto.com] WebSocket connected!";
            // 连接成功后订阅 ticker.BTC_USDT
            std::string sub = R"({"id":1,"method":"subscribe","params":{"channels":["ticker.BTC_USDT"]}})";
            wsPtr->getConnection()->send(sub);
        });

    // 15秒后退出
    //app().getLoop()->runAfter(6000, []() { app().quit(); });

    // 设置事件循环线程数，例如4线程
    app().setThreadNum(4);

    app().setLogLevel(trantor::Logger::kDebug);
    app().run();
    LOG_INFO << "bye!";
    return 0;
}

//g++ drogon_ws.cpp -o ws_drogon -std=c++20 \
  -I/opt/homebrew/include \
  -L/opt/homebrew/lib \
  -ldrogon -ltrantor -ljsoncpp \
  -lssl -lcrypto \
  -lz -lcares -lhiredis -lsqlite3 \
  -lpthread