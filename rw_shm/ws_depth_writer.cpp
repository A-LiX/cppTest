#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <cstdlib>
#include <ctime>

#include "shared_ring_buffer.hpp"
#include "depth.hpp"

using namespace drogon;

int main()
{
    app().setThreadNum(1);

    std::string binanceWsUrl = "wss://stream.binance.com:9443";
    std::string binancePath = "/stream?streams=btcusdt@depth20@100ms"; // 订阅 BTC 和 ETH

    static SharedRingBufferWriter<Depth> ringWriter("/depth_ring", 128);
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
            // 收到 Binance 行情，发布到 Redis 频道

            Json::Value root;
            Json::CharReaderBuilder builder;
            std::istringstream iss(message);
            std::string errs;
            if (!Json::parseFromStream(builder, iss, &root, &errs)) {
                LOG_ERROR << "JSON parse error: " << errs;
                return;
            }

            if (root.isMember("data") && root["data"].isMember("bids") && root["data"].isMember("asks")) {
                const auto& data = root["data"];
                if (data["bids"].size() > 0 && data["asks"].size() > 0) {
                    Depth d;
                    d.bid_price = std::stod(data["bids"][0][0].asString());
                    d.bid_volume = std::stod(data["bids"][0][1].asString());
                    d.ask_price = std::stod(data["asks"][0][0].asString());
                    d.ask_volume = std::stod(data["asks"][0][1].asString());
                    d.ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now().time_since_epoch()).count();

                    ringWriter.write(d);
                    LOG_INFO << "Depth pushed: bid=" << d.bid_price <<" v="<<d.bid_volume<< ", ask=" << d.ask_price<<" askv="<<d.ask_volume
                             << " @ts=" << d.ts;
                } else {
                    LOG_DEBUG << "No bids/asks found in depth update.";
                }
            } else {
                LOG_DEBUG << "Unexpected message format: " << message;
            } });

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

    connectToBinance();
    // 启动应用
    app().run();
    return 0;
}
