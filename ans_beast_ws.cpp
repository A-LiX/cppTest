#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>
#include <string>
#include <ctime>
#include <arpa/inet.h>     // for inet_pton, sockaddr_in, htons
#include <unistd.h>        // for close
#include <openssl/ssl.h>   // for SSL, SSL_CTX, SSL_new, etc.
#include <openssl/err.h>   // for ERR_print_errors_fp
// #include <anssock.h>    // DPDK-ANS socket头文件，实际项目需要包含

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

void send_ping(SSL* ssl) {
    unsigned char ping_frame[2] = {0x89, 0x00}; // Ping Frame: FIN=1, Opcode=0x9
    SSL_write(ssl, ping_frame, 2);
    std::cout << "[WS] Sent Ping\n";
}

void handle_pong(SSL* ssl, beast::flat_buffer& buffer) {
    unsigned char pong_frame[2] = {0x8A, 0x00}; // Pong Frame: FIN=1, Opcode=0xA
    SSL_read(ssl, (char*)buffer.prepare(4096).data(), 4096);  // Read incoming frame
    buffer.commit(2);  // Commit the PONG message bytes after reading
    std::cout << "[WS] Received Pong\n";
}

int main() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

    int fd = ans_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("ans_socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9443);
    inet_pton(AF_INET, "18.162.221.129", &addr.sin_addr);  // Binance IP

    if (ans_connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("ans_connect");
        return 1;
    }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // WebSocket 握手
    const char* ws_key = "x3JJHMbDL1EzLkh9GBhXDw==";
    std::string request =
        "GET /ws/btcusdt@trade HTTP/1.1\r\n"
        "Host: stream.binance.com:9443\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + std::string(ws_key) + "\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    SSL_write(ssl, request.c_str(), request.size());

    char buf[4096];
    int n = SSL_read(ssl, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    std::string resp(buf);
    if (resp.find("101 Switching Protocols") == std::string::npos) {
        std::cerr << "WebSocket handshake failed\n";
        return 1;
    }
    std::cout << "[+] WebSocket connected\n";

    beast::flat_buffer buffer;
    std::string message_buffer;
    bool assembling = false;
    bool send_ping_flag = false;

    while (true) {
        if (send_ping_flag) {
            send_ping(ssl);  // Ping
            send_ping_flag = false;
        }

        char* data = (char*)buffer.prepare(4096).data();
        int len = SSL_read(ssl, data, 4096);
        if (len <= 0) break;
        buffer.commit(len);

        while (buffer.size() > 0) {
            websocket::detail::frame_header fh;
            auto consumed = websocket::detail::read_fh(fh, buffer.data(), false);
            if (consumed == 0 || buffer.size() < consumed + fh.payload_len) break;

            buffer.consume(consumed);
            auto payload_buf = buffer.data();
            std::string payload;
            payload.reserve(fh.payload_len);
            auto it = boost::asio::buffers_begin(payload_buf);
            for (size_t i = 0; i < fh.payload_len && it != boost::asio::buffers_end(payload_buf); ++i, ++it)
                payload.push_back(*it);
            buffer.consume(fh.payload_len);

            if (fh.opcode == 1 || fh.opcode == 2) { // text/binary 起始帧
                message_buffer = payload;
                assembling = !fh.fin;
                if (fh.fin) std::cout << "[WS MSG] " << message_buffer << "\n";
            } else if (fh.opcode == 0 && assembling) { // continuation frame
                message_buffer += payload;
                if (fh.fin) {
                    assembling = false;
                    std::cout << "[WS MSG] " << message_buffer << "\n";
                }
            } else if (fh.opcode == 0x9) {  // Ping Frame
                handle_pong(ssl, buffer);  // Handle pong response
                send_ping_flag = true;     // Reset ping flag
            } else {
                std::cerr << "Unhandled opcode: " << fh.opcode << "\n";
            }
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
    SSL_CTX_free(ctx);
    return 0;
}
