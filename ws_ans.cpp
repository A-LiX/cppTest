#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include "ans_socket.h" // 假设这是你编译好的 ans socket 接口

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

void print_trade_data(const std::string& json_str) {
    try {
        json::value jv = json::parse(json_str);
        json::object obj = jv.as_object();
        
        std::cout << "Trade Data:" << std::endl;
        std::cout << "Symbol: " << obj["s"].as_string() << std::endl;
        std::cout << "Price: " << obj["p"].as_string() << std::endl;
        std::cout << "Quantity: " << obj["q"].as_string() << std::endl;
        std::cout << "Time: " << obj["T"].as_int64() << std::endl;
        std::cout << "-------------------" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
}

int main() {
    try {
        const char* host = "stream.binance.com";
        const char* port = "9443";
        const char* target = "/ws/btcusdt@trade";

        SSL_library_init();
        SSL_load_error_strings();
        const SSL_METHOD* method = TLS_client_method();
        SSL_CTX* ctx = SSL_CTX_new(method);

        if (!ctx) {
            std::cerr << "Failed to create SSL_CTX" << std::endl;
            return 1;
        }

        // 使用 ANS 建立 TCP socket
        int sockfd = ans_socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("ans_socket");
            return 1;
        }

        // DNS解析和连接（可替换为你自己的 connect 实现）
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(9443);
        inet_pton(AF_INET, "18.162.221.129", &server.sin_addr); // Binance IP（示例）

        if (ans_connect(sockfd, (sockaddr*)&server, sizeof(server)) < 0) {
            perror("ans_connect");
            return 1;
        }

        // 将 ANS 的 socket 转为 OpenSSL 的 BIO
        SSL* ssl = SSL_new(ctx);
        BIO* bio = BIO_new_socket(sockfd, BIO_NOCLOSE);
        SSL_set_bio(ssl, bio, bio);

        if (SSL_connect(ssl) <= 0) {
            std::cerr << "TLS handshake failed" << std::endl;
            ERR_print_errors_fp(stderr);
            return 1;
        }

        std::cout << "TLS handshake succeeded" << std::endl;

        // 设置 Boost.Beast 使用 ANS socket 和 SSL
        net::io_context ioc;
        net::ssl::context ssl_ctx(net::ssl::context::tlsv12_client);
        net::ssl::stream<net::ip::tcp::socket> stream(ioc, ssl_ctx);
        SSL_set_fd(stream.native_handle(), sockfd);

        websocket::stream<net::ssl::stream<net::ip::tcp::socket>> ws(std::move(stream));

        ws.handshake(host, target);
        std::cout << "WebSocket handshake succeeded" << std::endl;

        // Main loop to receive and process messages
        for (;;) {
            try {
                beast::flat_buffer buffer;
                ws.read(buffer);
                std::string message = beast::buffers_to_string(buffer.data());
                print_trade_data(message);
            } catch (const std::exception& e) {
                std::cerr << "Error reading message: " << e.what() << std::endl;
                break;
            }
        }

        // Cleanup
        ws.close(websocket::close_code::normal);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        ans_close(sockfd);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}