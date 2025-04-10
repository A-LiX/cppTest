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
#include <boost/asio/yield.hpp>              // 包含 Boost.Asio 协程支持

// Define SIMDJSON_PADDING if not already defined
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h> 

namespace beast = boost::beast;         // Boost.Beast 命名空间
namespace websocket = beast::websocket; // WebSocket 相关
namespace net = boost::asio;            // Boost.Asio 命名空间
namespace ssl = boost::asio::ssl;       // SSL 命名空间
using tcp = net::ip::tcp;               // TCP

struct CoroutineHandler : public boost::asio::coroutine {
    using error_code = boost::system::error_code;

    net::io_context &ioc;
    websocket::stream<beast::ssl_stream<tcp::socket>> &ws;
    beast::flat_buffer buffer;
    simdjson::ondemand::parser parser;

    CoroutineHandler(net::io_context &ioc, websocket::stream<beast::ssl_stream<tcp::socket>> &ws)
        : ioc(ioc), ws(ws) {}

    void operator()(error_code ec = {}, std::size_t bytes_transferred = 0) {
        reenter(this) {
            while (true) {
                // 读取数据
                yield ws.async_read(buffer, std::move(*this));
                if (ec) {
                    spdlog::error("WebSocket read failed: {}", ec.message());
                    return;
                }

                // 检查缓冲区是否有数据
                if (buffer.size() == 0) {
                    spdlog::warn("Empty buffer received, skipping parsing...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue; // 如果缓冲区为空，跳过解析
                }

                auto data = static_cast<const char *>(buffer.data().data());
                std::cout << "Received data: " << data << std::endl;
                size_t size = buffer.size();
                simdjson::padded_string padded_data(data, size);
                auto doc_result = parser.iterate(padded_data);

                if (doc_result.error()) {
                    spdlog::error("JSON Parsing Error: {}", simdjson::error_message(doc_result.error()));
                    buffer.consume(buffer.size());
                    continue;
                }

                auto &doc = doc_result.value_unsafe();

                // 提取字段值
                auto event_type_result = doc["e"].get_string();
                auto symbol_result = doc["s"].get_string();
                auto price_result = doc["p"].get_string();

                std::string_view event_type = event_type_result.value_unsafe();
                std::string_view symbol = symbol_result.value_unsafe();
                double price = std::stod(std::string(price_result.value_unsafe()));

                spdlog::trace("Parsed Message:");
                spdlog::trace("  Event Type: {}", event_type);
                spdlog::trace("  Symbol: {}", symbol);
                spdlog::trace("  Price: {}", price);

                // 清空缓冲区
                buffer.consume(buffer.size());
            }
        }
    }
};

int main() {
    // 配置 spdlog 异步日志（队列大小为 8192）
    spdlog::init_thread_pool(8192, 1); // 日志队列大小为 8192，1 个后台线程处理日志

    // 配置滚动日志（按时间滚动：1天）
    auto daily_logger = spdlog::create_async<spdlog::sinks::daily_file_sink_mt>(
        "daily_logger", "./boost_client_coroutine.log", 0, 0); // 每天午夜滚动
    spdlog::set_default_logger(daily_logger);
    spdlog::set_level(spdlog::level::debug); // 设置日志等级为 debug
    spdlog::flush_on(spdlog::level::debug);  // 确保 debug 以上的日志立即写入文件

    spdlog::info("Starting WebSocket client...");

    // 创建 io_context
    net::io_context ioc;

    // 设置 SSL 上下文，使用 TLSv1.2 客户端
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none);

    // 创建并配置 WebSocket 流对象
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

    // DNS 解析
    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve("stream.binance.com", "9443");
    spdlog::info("DNS resolution succeeded");

    // 连接到目标 TCP 端点
    net::connect(ws.next_layer().next_layer(), results.begin(), results.end());
    spdlog::info("TCP connection succeeded");

    // 执行 SSL 握手
    ws.next_layer().handshake(ssl::stream_base::client);
    spdlog::info("SSL handshake succeeded");

    // 执行 WebSocket 握手
    ws.handshake("stream.binance.com", "/ws/btcusdt@trade");
    spdlog::info("WebSocket handshake succeeded");

    spdlog::info("Connected to Binance WebSocket at stream.binance.com");

    // 启动协程
    CoroutineHandler handler(ioc, ws);
    handler(boost::system::error_code());

    // 运行 io_context
    ioc.run();

    // 关闭连接
    ws.close(websocket::close_code::normal);
    spdlog::info("WebSocket connection closed successfully");

    return EXIT_SUCCESS;
}