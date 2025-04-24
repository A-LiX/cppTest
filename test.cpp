#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

int main() {
    try {
        net::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();

        // 解析主机地址
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve("stream.binance.com", "9443");

        // 建立加密连接（SSL + TCP）
        beast::ssl_stream<beast::tcp_stream> ssl_stream(ioc, ctx);
        beast::get_lowest_layer(ssl_stream).connect(results);
        ssl_stream.handshake(ssl::stream_base::client);

        // 建立 WebSocket 加密连接
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(std::move(ssl_stream));
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent, "BoostWSClient");
            }));
        ws.handshake("stream.binance.com", "/ws/btcusdt@depth");

        // 开始接收数据
        beast::flat_buffer buffer;
        while (true) {
            ws.read(buffer);
            std::cout << beast::make_printable(buffer.data()) << std::endl;
            buffer.consume(buffer.size());
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}