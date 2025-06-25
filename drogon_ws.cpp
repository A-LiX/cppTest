#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // Binance 行情 WebSocket 地址，单币对不使用 streams
    std::string server = "wss://stream.binance.com:9443";
    std::string path = "/ws/btcusdt@ticker";

    auto wsPtr = WebSocketClient::newWebSocketClient(server);
    auto req = HttpRequest::newHttpRequest();
    req->setPath(path);
    // 不需要手动添加 Host 头

    wsPtr->setMessageHandler([](const std::string &message,
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

        LOG_INFO << "Binance MultiStream (" << messageType << "): " << message;
    });

    wsPtr->setConnectionClosedHandler([](const WebSocketClientPtr &) {
        LOG_INFO << "WebSocket connection closed!";
    });

    LOG_INFO << "Connecting to Binance WebSocket at " << server << path;
    wsPtr->connectToServer(
        req,
        [wsPtr](ReqResult r,
           const HttpResponsePtr &,
           const WebSocketClientPtr &) {
            if (r != ReqResult::Ok)
            {
                LOG_ERROR << "Failed to establish WebSocket connection!";
                wsPtr->stop();
                return;
            }
            LOG_INFO << "WebSocket connected!";
            // 不需要再发送 SUBSCRIBE 消消息
        });

    // 60秒后退出
    //app().getLoop()->runAfter(60, []() { app().quit(); });

    app().setLogLevel(trantor::Logger::kDebug);
    app().run();
    LOG_INFO << "bye!";
    return 0;
}