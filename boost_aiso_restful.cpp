#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <iostream>

using namespace drogon;

// 轮询行情的函数
void poll_symbols() {
    static std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "BNBUSDT", "SOLUSDT", "XRPUSDT", "DOGEUSDT"};
    for (const auto& symbol : symbols) {
        auto client = HttpClient::newHttpClient("https://api.binance.com");
        auto req = HttpRequest::newHttpRequest();
        req->setMethod(Get);
        req->setPath("/api/v3/ticker/price");
        req->setParameter("symbol", symbol);
        std::cout << "[LOG] 发送请求到 Binance 获取" << symbol << " ..." << std::endl;
        client->sendRequest(req, [symbol](ReqResult result, const HttpResponsePtr &resp)
                            {
            if (result == ReqResult::Ok && resp->getStatusCode() == k200OK) {
                std::cout << "[LOG] " << symbol << " 原始响应: " << resp->getBody() << std::endl;
            } else {
                std::cout << "[LOG] 获取" << symbol << "行情失败" << std::endl;
            } });
    }
    // 5秒后再次轮询
    app().getLoop()->runAfter(100.0, poll_symbols);
}

int main()
{
    std::cout << "[LOG] 程序启动" << std::endl;
    app().setThreadNum(4);
    app().setLogLevel(trantor::Logger::kTrace);

    // 启动事件循环后开始轮询
    app().getLoop()->queueInLoop(poll_symbols);
    app().getLoop()->queueInLoop(poll_symbols);

    poll_symbols();
    app().run();
    return 0;
}

