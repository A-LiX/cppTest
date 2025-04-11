#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <simdjson.h>
#include <spdlog/spdlog.h>                   // 引入 spdlog
#include <spdlog/sinks/basic_file_sink.h>    // 引入文件日志 sink
#include <spdlog/sinks/rotating_file_sink.h> // 引入滚动文件日志 sink
#include <spdlog/sinks/daily_file_sink.h>    // 引入按时间滚动日志 sink
#include <spdlog/async.h>                    // 引入异步日志支持

// Define SIMDJSON_PADDING if not already defined

#include <iostream>
#include <string>

namespace beast = boost::beast;         // Boost.Beast 命名空间
namespace websocket = beast::websocket; // WebSocket 相关
namespace net = boost::asio;            // Boost.Asio 命名空间
namespace ssl = boost::asio::ssl;       // SSL 命名空间
using tcp = net::ip::tcp;               // TCP

#define TRADE_BUFFER_SIZE 256

int main()
{

#if defined(__linux__)
#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
        // 设置 CPU 亲和性
    cpu_set_t mask;
    CPU_ZERO(&mask);   // 清空 CPU 集合
    CPU_SET(1, &mask); // 设置 CPU 核心 1
    // 设置当前进程的 CPU 亲和性
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
    {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
#endif

    // 配置 spdlog 异步日志（队列大小为 8192）
    spdlog::init_thread_pool(8192, 1); // 日志队列大小为 8192，1 个后台线程处理日志

    // 配置滚动日志（按时间滚动：1周）
    auto weekly_logger = spdlog::create_async<spdlog::sinks::daily_file_sink_mt>(
        "weekly_logger", "./boost_client.log", 0, 0); // 每天午夜滚动
    spdlog::set_default_logger(weekly_logger);
    spdlog::set_level(spdlog::level::debug); // 设置日志等级为 debug
    spdlog::flush_on(spdlog::level::debug);  // 确保 debug 以上的日志立即写入文件

    spdlog::info("Starting WebSocket client...");

    // 1. 创建 io_context
    net::io_context ioc;
    spdlog::info("Created io_context");

    // 2. 设置 SSL 上下文，使用 TLSv1.2 客户端
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none);
    spdlog::info("Configured SSL context");

    // 3. 定义服务器信息和目标
    const std::string host = "stream.binance.com";
    const std::string port = "9443";
    // const std::string target = "/ws/btcusdt@depth@100ms"; // 目标路径
    const std::string target = "/ws/btcusdt@trade"; // 目标路径
    spdlog::info("Defined server information: host={}, port={}, target={}", host, port, target);

    // 4. DNS 解析
    tcp::resolver resolver(ioc);
    boost::system::error_code err;
    auto const results = resolver.resolve(host, port, err);
    if (err)
    {
        spdlog::error("DNS resolution failed: {}", err.message());
        return EXIT_FAILURE;
    }
    spdlog::info("DNS resolution succeeded");

    // 5. 创建 Boost.Beast SSL WebSocket 流对象
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);
    spdlog::info("Created WebSocket stream");

    // 7. 连接到目标 TCP 端点
    net::connect(ws.next_layer().next_layer(), results.begin(), results.end(), err);
    if (err)
    {
        spdlog::error("TCP connection failed: {}", err.message());
        return EXIT_FAILURE;
    }
    spdlog::info("TCP connection succeeded");

    // 8. 执行 SSL 握手
    ws.next_layer().handshake(ssl::stream_base::client, err);
    if (err)
    {
        spdlog::error("SSL handshake failed: {}", err.message());
        return EXIT_FAILURE;
    }
    spdlog::info("SSL handshake succeeded");

    // 9. 执行 WebSocket 握手
    ws.handshake(host, target, err);
    if (err)
    {
        spdlog::error("WebSocket handshake failed: {}", err.message());
        return EXIT_FAILURE;
    }
    spdlog::info("WebSocket handshake succeeded");

    spdlog::info("Connected to Binance WebSocket at {}", host);

    // 10. 循环读取并解析消息
    beast::flat_buffer buffer;
    buffer.prepare(TRADE_BUFFER_SIZE); // 使用宏定义的 buffer 大小
    simdjson::ondemand::parser parser; // 创建 simdjson 解析器
    while (true)
    {
        // 在 while 循环中实现忙等待
        while (ws.next_layer().next_layer().available() == 0)
        {
            //std::this_thread::sleep_for(std::chrono::nanoseconds(100)); // 短暂休眠以避免 100% CPU 占用
        }

        // 读取消息
        ws.read(buffer, err);
        if (err)
        {
            spdlog::error("WebSocket read failed: {}", err.message());
            return EXIT_FAILURE;
        }

        size_t size = buffer.size();
        auto data = static_cast<char *>(buffer.data().data());

        // simdjson::padded_string padded_data(data, size);
        // auto doc_result = parser.iterate(data, size);
        auto doc_result = parser.iterate(data, size, TRADE_BUFFER_SIZE);
        if (doc_result.error())
        {
            spdlog::error("JSON Parsing Error: {}", simdjson::error_message(doc_result.error()));
            buffer.consume(size);
            continue;
        }

        std::cout << "Received message: " << doc_result.value_unsafe() << std::endl;
        auto &doc = doc_result.value_unsafe();

        // // 提取字段值
        std::string_view event_type = doc["e"].get_string().value_unsafe();
        std::string_view symbol = doc["s"].get_string().value_unsafe();
        // double price = std::stod(std::string(doc["p"].get_string().value_unsafe()));

        spdlog::trace("Parsed Message:");
        spdlog::trace("  Event Type: {}", event_type);
        spdlog::trace("  Symbol: {}", symbol);
        // spdlog::trace("  Price: {}", price);

        buffer.consume(size);
    }

    // 11. 关闭连接
    ws.close(websocket::close_code::normal, err);
    if (err)
    {
        spdlog::error("WebSocket close failed: {}", err.message());
        return EXIT_FAILURE;
    }
    spdlog::info("WebSocket connection closed successfully");

    return EXIT_SUCCESS;
}
