#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <json/json.h>

using namespace drogon;
using namespace std::chrono_literals;

int main() {
    // 设置日志级别（可选）
    trantor::Logger::setLogLevel(trantor::Logger::kDebug);

    // 创建WebSocket客户端
    auto wsClient = WebSocketClient::newWebSocketClient("wss://stream.binance.com:9443");
    // 启用SSL证书验证

    // 创建请求对象
    HttpRequestPtr req = HttpRequest::newHttpRequest();
    req->setPath("/ws/btcusdt@trade"); // 订阅BTC/USDT实时交易流

    // 连接并订阅
    wsClient->setMessageHandler([](const std::string& message,
                                  const WebSocketClientPtr& client,
                                  const WebSocketMessageType& type) {
        // 确保是文本消息
        if (type == WebSocketMessageType::Text) {
            try {
                // 解析JSON
                Json::Value jsonData;
                Json::Reader reader;
                if (reader.parse(message, jsonData)) {
                    // 提取交易数据
                    auto symbol = jsonData["s"].asString();
                    auto price = jsonData["p"].asString();
                    auto quantity = jsonData["q"].asString();
                    auto tradeTime = jsonData["T"].asUInt64();
                    
                    // 打印交易信息（实际应用中应处理数据）
                    LOG_INFO << "交易对: " << symbol
                             << " | 价格: " << price
                             << " | 数量: " << quantity
                             << " | 时间: " << tradeTime;
                } else {
                    LOG_ERROR << "JSON解析失败: " << message;
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "处理错误: " << e.what();
            }
        }
    });

    // 设置连接关闭处理
    wsClient->setConnectionClosedHandler([](const WebSocketClientPtr& client) {
        LOG_WARN << "WebSocket连接已关闭";
    });

    // 发起连接
    wsClient->connectToServer(
        req,
        [](ReqResult r, const HttpResponsePtr&, const WebSocketClientPtr& ws) {
            if (r != ReqResult::Ok) {
                LOG_ERROR << "连接失败: " << (int)r;
                app().quit();
                return;
            }
            LOG_INFO << "成功连接到Binance WebSocket";
        });

    // 运行主事件循环
    app().setLogLevel(trantor::Logger::kWarn);
    app().run();
    return 0;
}