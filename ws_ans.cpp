#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <simdjson.h>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <netdb.h> // For gethostbyname

// 如果是 macOS (Apple 平台)，则使用系统原生 socket；否则（如 Linux）使用 ANS socket 实现
#ifdef __linux__
#include "ans_socket.h"
#define socket ans_socket
#define connect ans_connect
#define close ans_close
#define read ans_read
#define write ans_write
#endif

// g++ -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib  -lboost_system -lboost_thread -lssl -lcrypto -lpthread -lsimdjson -o ws_ans ws_ans.cpp

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

int main()
{
    try
    {
        const char *host = "stream.binance.com";
        const char *port = "9443";
        const char *target = "/ws/btcusdt@trade";

        SSL_library_init();
        SSL_load_error_strings();
        const SSL_METHOD *method = TLS_client_method();
        SSL_CTX *ctx = SSL_CTX_new(method);

        if (!ctx)
        {
            std::cerr << "Failed to create SSL_CTX" << std::endl;
            return 1;
        }

        // 使用 ANS 建立 TCP socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("ans_socket");
            return 1;
        }

        // DNS解析和连接
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(9443);

        // 域名解析
        struct hostent *he = gethostbyname(host);
        if (!he)
        {
            std::cerr << "DNS lookup failed for " << host << std::endl;
            return 1;
        }
        memcpy(&server.sin_addr, he->h_addr, he->h_length);

        if (connect(sockfd, (sockaddr *)&server, sizeof(server)) < 0)
        {
            perror("connect");
            return 1;
        }

        // 推荐做法：不要再用 OpenSSL 的 SSL_new/SSL_connect/SSL_set_bio
        // 只用 Boost.Asio 的 ssl::stream 做 TLS 握手

        net::io_context ioc;
        net::ssl::context ssl_ctx(net::ssl::context::tlsv12_client);

        tcp::socket sock(ioc);
        sock.assign(tcp::v4(), sockfd);

        net::ssl::stream<tcp::socket> stream(std::move(sock), ssl_ctx);
        stream.handshake(net::ssl::stream_base::client);

        websocket::stream<net::ssl::stream<tcp::socket>> ws(std::move(stream));
        ws.handshake(host, target);
        std::cout << "WebSocket handshake succeeded" << std::endl;

        beast::flat_buffer buffer;
        for (;;)
        {
            ws.read(buffer);
            std::string message = beast::buffers_to_string(buffer.data());
            std::cout << message << std::endl;

            // 使用 simdjson 解析 "E" 字段并计算延迟
            simdjson::ondemand::parser parser;
            auto doc_result = parser.iterate(message);
            if (!doc_result.error()) {
                auto& doc = doc_result.value_unsafe();
                auto event_time_result = doc["E"].get_int64();
                if (!event_time_result.error()) {
                    int64_t event_time = event_time_result.value_unsafe();
                    // 获取本地当前时间（毫秒）
                    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    std::cout << "Event Time: " << event_time << ", Now: " << now_ms
                              << ", Delay(ms): " << (now_ms - event_time) << std::endl;
                }
            }

            buffer.consume(buffer.size()); // 清空buffer，防止累积多帧
        }

        // Cleanup
        ws.close(websocket::close_code::normal);
        close(sockfd);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}