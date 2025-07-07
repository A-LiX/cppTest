#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <trantor/utils/Logger.h>
#include <chrono>

using namespace drogon;

int main()
{
    app().setThreadNum(1);

    app().createRedisClient("127.0.0.1", 6379);

    std::string redisStream = "my_stream";
    const std::string binanceWsUrl = "wss://stream.binance.com:9443";
    const std::string binancePath = "/stream?streams=btcusdt@trade/btcusdt@depth20@100ms";

    // 使用 shared_ptr 保持连接持久存在
    std::shared_ptr<WebSocketClient> wsClient = nullptr;

    std::function<void()> connectToBinance;

    connectToBinance = [&]()
    {
        wsClient = WebSocketClient::newWebSocketClient(binanceWsUrl);
        auto wsReq = HttpRequest::newHttpRequest();
        wsReq->setPath(binancePath);

        wsClient->setMessageHandler([=](const std::string &message,
                                        const WebSocketClientPtr &wsConn,
                                        const WebSocketMessageType &type)
                                    {
            if (type == WebSocketMessageType::Ping) {
                wsConn->getConnection()->send(std::string(), WebSocketMessageType::Pong);
                LOG_INFO << "[WS] Received Ping -> sent Pong.";
                //wsClient->stop();
                return;
            }

            if (type != WebSocketMessageType::Text) return;

            std::string redisChannel = "my_channel";
            auto redisClient = app().getRedisClient();
            if (!redisClient) {
            LOG_ERROR << "RedisClient is nullptr!";
            return;
            }
            redisClient->execCommandAsync(
                [redisChannel](const drogon::nosql::RedisResult &result) {
            // LOG_INFO << "Published Binance trade to channel '" << redisChannel
            //          << "'. Subscriber count: " << result.asInteger();
                },
                [](const std::exception &e) {
            LOG_ERROR << "Failed to publish: " << e.what();
                },
                "PUBLISH %s %s", redisChannel.c_str(), message.c_str()); });
        wsClient->setConnectionClosedHandler([&connectToBinance, &wsClient](const WebSocketClientPtr &)
                                             {
                                                LOG_ERROR << "Binance WebSocket closed!";
                                                wsClient->stop();
                                                // 重新连接
                                                LOG_INFO << "Reconnecting to Binance WebSocket...";
                                                app().getLoop()->runInLoop(connectToBinance);
                                                //connectToBinance();
                                                return; });

        // 这里处理 Text 类型的数据
        // LOG_INFO << "[WS] Received: " << message.substr(0, 128) << "...";

        wsClient->setConnectionClosedHandler([=](const WebSocketClientPtr &)
                                             {
            LOG_ERROR << "[WS] Connection closed. Reconnecting in 3s...";
            app().getLoop()->runAfter(0.0, connectToBinance); });

        wsClient->connectToServer(wsReq, [=](ReqResult r,
                                             const HttpResponsePtr &,
                                             const WebSocketClientPtr &wsPtr)
                                  {
            if (r != ReqResult::Ok) {
                LOG_ERROR << "[WS] Failed to connect.";
                app().getLoop()->runAfter(3.0, connectToBinance);
                return;
            }
            LOG_INFO << "[WS] Connected to Binance: " << wsReq->getPath(); });
    };

    connectToBinance();
    app().run();
}