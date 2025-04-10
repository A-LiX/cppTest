#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <cstdlib>
#include <string>
#include <memory>
#include <asio/ssl/context.hpp> // 使用 standalone Asio 的 SSL 上下文

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

using websocketpp::connection_hdl;

// 添加 TLS 初始化回调
std::shared_ptr<asio::ssl::context> on_tls_init(connection_hdl) {
    auto ctx = std::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    try {
        // 配置 OpenSSL 使用系统 CA 证书
        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::single_dh_use);
        ctx->set_default_verify_paths();
        ctx->set_verify_mode(asio::ssl::verify_peer);
    } catch (std::exception &e) {
        std::cout << "TLS initialization error: " << e.what() << std::endl;
    }
    return ctx;
}

// 定义消息处理回调
void on_message(connection_hdl, client::message_ptr msg) {
    std::cout << "Received: " << msg->get_payload() << std::endl;
}

int main() {
    client c;
    try {
        // 禁用默认日志输出
        c.clear_access_channels(websocketpp::log::alevel::all);
        c.init_asio();
        // 设置 TLS 初始化处理器
        c.set_tls_init_handler([](connection_hdl hdl) {
            return on_tls_init(hdl);
        });
        c.set_message_handler(&on_message);

        // 使用安全连接 URL（wss://）
        std::string uri = "wss://stream.binance.com:9443/ws/btcusdt@trade";

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "Could not create connection because: " << ec.message() << std::endl;
            return EXIT_FAILURE;
        }

        c.connect(con);
        std::cout << "Client running, waiting for Binance data..." << std::endl;
        c.run();
    } catch (websocketpp::exception const &e) {
        std::cout << "WebSocket++ exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cout << "Other exception" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
