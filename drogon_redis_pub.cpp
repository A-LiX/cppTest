#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <cstdlib>
#include <ctime>

using namespace drogon;

int main()
{
    app().setThreadNum(1);
    app().createRedisClient("127.0.0.1", 6379);

    std::string redisStream = "my_stream";
    std::string binanceWsUrl = "wss://stream.binance.com:9443";
    std::string binancePath = "/stream?streams=btcusdt@trade/btcusdt@depth20@100ms"; // 订阅 BTC 和 ETH

    std::function<void()> connectToBinance;

    connectToBinance = [&]()
    {
        auto wsClient = WebSocketClient::newWebSocketClient(binanceWsUrl);
        auto wsReq = HttpRequest::newHttpRequest();
        wsReq->setPath(binancePath);

        wsClient->setMessageHandler([wsClient, &connectToBinance](const std::string &message,
                                                                  const WebSocketClientPtr &wsConn,
                                                                  const WebSocketMessageType &type)
                                    {

            if (type == WebSocketMessageType::Ping){
                wsConn->getConnection()->send(std::string(), WebSocketMessageType::Pong);
                LOG_INFO << "Received Ping, sent Pong.";
                return;
            }
            if (type != WebSocketMessageType::Text) return;
            std::string redisChannel = "my_channel";
            // 收到 Binance 行情，发布到 Redis 频道
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

        wsClient->connectToServer(
            wsReq,
            [&](ReqResult r, const HttpResponsePtr &, const WebSocketClientPtr &wsPtr)
            {
                if (r != ReqResult::Ok)
                {
                    LOG_ERROR << "Failed to connect to Binance WebSocket!";
                    wsPtr->stop();
                    app().quit();
                    return;
                }
                LOG_INFO << "Connected to wss://stream.binance.com:9443" << wsReq->getPath() << " WebSocket!";
            });
    };

    //app().getLoop()->runEvery(5, []()
    //                         { LOG_INFO << "Starting Binance WebSocket connection..."; });
    // app().getLoop()->runAfter(0, connectToBinance);
    connectToBinance();
    // 启动应用
    app().run();
    return 0;
}

//g++ drogon_redis_pub.cpp -std=c++17 -O3\
 -I/opt/homebrew/include \
 -L/opt/homebrew/lib \
 -ldrogon -ltrantor -ljsoncpp -lhiredis \
 -lssl -lcrypto \
 -lz -lpthread \
 -lsqlite3 \
 -lcares \
 -o suber