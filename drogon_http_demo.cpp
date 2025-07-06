#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <iostream>
#include <chrono>
#include <thread>

using namespace drogon;

int main() {
    std::cout << "[LOG] 程序启动" << std::endl;
    
    // 注册获取币安BTCUSDT价格的处理器
    app().registerHandler("/binance_price",
        [](const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback) {
            std::cout << "[LOG] 收到请求: /binance_price" << std::endl;
            
            // 创建异步HTTP客户端
            auto client = HttpClient::newHttpClient("https://api.binance.com");
            std::cout << "[LOG] 创建HTTP客户端成功" << std::endl;
            
            // 请求BTCUSDT最新价格
            auto reqBinance = HttpRequest::newHttpRequest();
            reqBinance->setPath("/api/v3/ticker/price?symbol=BTCUSDT");
            std::cout << "[LOG] 准备发送请求到Binance, 路径: " << reqBinance->getPath() << std::endl;
            
            client->sendRequest(reqBinance, [callback](ReqResult result, const HttpResponsePtr &resp) {
                std::cout << "[LOG] 收到Binance响应, 结果代码: " << (int)result << std::endl;
                
                if(result == ReqResult::Ok && resp->getStatusCode() == k200OK) {
                    std::cout << "[LOG] Binance请求成功, 状态码: 200" << std::endl;
                    std::cout << "[LOG] 响应内容: " << resp->getBody() << std::endl;
                    // 直接返回Binance响应内容
                    auto out = HttpResponse::newHttpResponse();
                    out->setStatusCode(k200OK);
                    out->setContentTypeCode(CT_APPLICATION_JSON);
                    std::cout << "[LOG] 返回响应给客户端" << std::endl;
                    callback(out);
                } else {
                    std::cout << "[LOG] Binance请求失败或状态码非200, 状态码: " 
                              << (resp ? resp->getStatusCode() : 0) << std::endl;
                    if (resp) {
                        std::cout << "[LOG] 错误响应内容: " << resp->getBody() << std::endl;
                    }
                    auto out = HttpResponse::newHttpResponse();
                    out->setStatusCode(k500InternalServerError);
                    out->setContentTypeCode(CT_TEXT_PLAIN);
                    out->setBody("Failed to fetch Binance price");
                    std::cout << "[LOG] 返回错误响应给客户端" << std::endl;
                    callback(out);
                }
            });
        },
        {Get});

    // 注册一个简单的健康检查接口
    app().registerHandler("/health",
        [](const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback) {
            std::cout << "[LOG] 收到健康检查请求" << std::endl;
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->setBody("OK");
            callback(resp);
        },
        {Get});

    // 设置日志级别
    app().setLogLevel(trantor::Logger::kTrace);
    
    // 设置监听地址和端口
    app().addListener("0.0.0.0", 8080);
    std::cout << "[LOG] 服务器监听在 0.0.0.0:8080" << std::endl;
    std::cout << "[LOG] 可用接口: /binance_price, /health" << std::endl;
    
    // 启动服务器
    app().run();
    return 0;
}