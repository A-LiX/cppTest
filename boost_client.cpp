#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <simdjson.h>

#include <iostream>
#include <string>
#include <chrono>

// 命名空间别名
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

#define TRADE_BUFFER_SIZE 256

int main()
{
    // 1. 创建 io_context
    net::io_context ioc;

    // 2. 设置 SSL 上下文，使用 TLSv1.2 客户端
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none);

    // 3. 定义服务器信息和目标
    const std::string host = "stream.binance.com";
    const std::string port = "9443";
    const std::string target = "/ws/btcusdt@trade"; // WebSocket 路径

    // 4. DNS 解析
    tcp::resolver resolver(ioc);
    boost::system::error_code err;
    auto const results = resolver.resolve(host, port, err);
    if (err)
    {
        std::cerr << "DNS resolution failed: " << err.message() << std::endl;
        return EXIT_FAILURE;
    }

    // 5. 创建 Boost.Beast SSL WebSocket 流对象
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

    // 6. 连接到目标 TCP 端点
    net::connect(ws.next_layer().next_layer(), results.begin(), results.end(), err);
    if (err)
    {
        std::cerr << "TCP connection failed: " << err.message() << std::endl;
        return EXIT_FAILURE;
    }

    // 7. 执行 SSL 握手
    ws.next_layer().handshake(ssl::stream_base::client, err);
    if (err)
    {
        std::cerr << "SSL handshake failed: " << err.message() << std::endl;
        return EXIT_FAILURE;
    }

    // 8. 执行 WebSocket 握手
    ws.handshake(host, target, err);
    if (err)
    {
        std::cerr << "WebSocket handshake failed: " << err.message() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Connected to Binance WebSocket at " << host << std::endl;

    // 9. 循环读取并解析消息
    beast::flat_buffer buffer;
    simdjson::ondemand::parser parser;

    while (true)
    {
        // 忙等待直到有数据可读
        while (ws.next_layer().next_layer().available() == 0)
        {
            // 可选：短暂休眠以减少 CPU 占用
            // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }

        ws.read(buffer, err);
        if (err)
        {
            std::cerr << "WebSocket read failed: " << err.message() << std::endl;
            return EXIT_FAILURE;
        }

        size_t size = buffer.size();
        auto data = static_cast<char *>(buffer.data().data());

        auto start = std::chrono::high_resolution_clock::now();
        auto doc_result = parser.iterate(data, size, TRADE_BUFFER_SIZE);
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        std::cout << "Parsing time: " << elapsed_us << " us" << std::endl;

        if (doc_result.error())
        {
            std::cerr << "JSON Parsing Error: " << simdjson::error_message(doc_result.error()) << std::endl;
            buffer.consume(size);
            continue;
        }

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

        auto &doc = doc_result.value_unsafe();

        // 提取字段值（示例）
        std::string_view event_type = doc["e"].get_string().value_unsafe();
        std::string_view symbol = doc["s"].get_string().value_unsafe();
        int64_t trade_time_ms = doc["T"].get_int64().value_unsafe();

        std::cout << "Event Type: " << event_type << ", Symbol: " << symbol << std::endl;
        int64_t latency_ms = now_ms - trade_time_ms;
        std::cout << ", Latency (us): " << latency_ms << std::endl;

        buffer.consume(size);
    }

    // 10. 关闭连接（不会执行到此处，因为上面是死循环）
    ws.close(websocket::close_code::normal, err);
    if (err)
    {
        std::cerr << "WebSocket close failed: " << err.message() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}