#include <drogon/drogon.h>
#include <drogon/nosql/RedisSubscriber.h>
#include <trantor/net/EventLoop.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace drogon;

int main() {
    // 初始化 spdlog 控制台异步日志
    auto async_console = spdlog::stdout_color_mt<spdlog::async_factory>("async_console");
    spdlog::set_default_logger(async_console);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_level(spdlog::level::debug);

    app().setThreadNum(1);
    app().createRedisClient("127.0.0.1", 6379);

    // 推荐用 RedisSubscriber 进行订阅，参考官方 ws demo
    app().getLoop()->runAfter(0, [] {
        auto redisClient = app().getRedisClient();
        if (!redisClient) {
            SPDLOG_ERROR("RedisClient is nullptr!");
            return;
        }
        // 创建 RedisSubscriber
        auto subscriber = redisClient->newSubscriber();
        std::string channel = "my_channel";
        // 这里的回调会被每条消息多次调用，相当于事件驱动的“循环”
        subscriber->subscribe(
            channel,
            [channel](const std::string &subChannel, const std::string &msg) {
                SPDLOG_INFO("接收到消息：[{}] -> {}", subChannel, msg);
                // 这里每收到一条消息就会自动再次进入回调，相当于循环处理
            }
        );
        // subscriber 生命周期由 lambda 持有，防止提前析构
        static auto keepAlive = subscriber;
    });

    app().run();
    return 0;
}