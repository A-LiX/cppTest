#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// 构造 WebSocket 握手请求
std::string build_ws_handshake(const std::string &host, const std::string &path)
{
    return "GET " + path + " HTTP/1.1\r\n"
                           "Host: " +
           host + "\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n"
                  "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                  "Sec-WebSocket-Version: 13\r\n"
                  "\r\n";
}

// 极致性能循环缓冲区 WebSocket 帧解析
class WSBuffer
{
public:
    static constexpr size_t BUF_SIZE = 1 << 16; // 64KB
    char buf[BUF_SIZE];
    size_t start = 0, end = 0;
    SSL *ssl = nullptr; // 用于自动回复 pong

    void set_ssl(SSL *s) { ssl = s; }

    // 追加新数据
    void append(const char *data, size_t len)
    {
        if (end + len <= BUF_SIZE)
        {
            memcpy(buf + end, data, len);
            end += len;
        }
        else
        {
            size_t remain = end - start;
            if (remain + len > BUF_SIZE)
            {
                std::cerr << "WSBuffer overflow!\n";
                exit(1);
            }
            memmove(buf, buf + start, remain);
            start = 0;
            end = remain;
            memcpy(buf + end, data, len);
            end += len;
        }
    }

    // 解析并打印所有完整帧，处理 ping/pong/close
    void parse_and_print()
    {
        size_t pos = start;
        while (end - pos >= 2)
        {
            unsigned char b1 = buf[pos];
            unsigned char b2 = buf[pos + 1];
            bool fin = b1 & 0x80;
            unsigned char opcode = b1 & 0x0f;
            bool mask = b2 & 0x80;
            uint64_t payload_len = b2 & 0x7f;
            size_t header_len = 2;

            if (payload_len == 126)
            {
                if (end - pos < 4)
                    break;
                payload_len = ((unsigned char)buf[pos + 2] << 8) | (unsigned char)buf[pos + 3];
                header_len += 2;
            }
            else if (payload_len == 127)
            {
                if (end - pos < 10)
                    break;
                payload_len = 0;
                for (int i = 0; i < 8; ++i)
                {
                    payload_len = (payload_len << 8) | (unsigned char)buf[pos + 2 + i];
                }
                header_len += 8;
            }

            unsigned char mask_key[4] = {0};
            if (mask)
            {
                if (end - pos < header_len + 4)
                    break;
                for (int i = 0; i < 4; ++i)
                    mask_key[i] = buf[pos + header_len + i];
                header_len += 4;
            }

            if (end - pos < header_len + payload_len)
                break; // 数据还不完整

            // 文本帧
            if (opcode == 1)
            {
                std::string payload;
                payload.reserve(payload_len);
                for (uint64_t i = 0; i < payload_len; ++i)
                {
                    char c = buf[pos + header_len + i];
                    if (mask)
                        c ^= mask_key[i % 4];
                    payload.push_back(c);
                }
                // std::cout << "WebSocket Text Frame: " << payload << std::endl;
            }
            // ping帧，自动回复pong
            else if (opcode == 9)
            {
                if (ssl)
                {
                    // 构造pong帧（opcode=0xA，payload同ping）
                    size_t pong_len = header_len + payload_len;
                    char pong[BUF_SIZE];
                    pong[0] = 0x8A; // FIN=1, opcode=0xA
                    // mask=0, payload_len同ping
                    if (payload_len < 126)
                    {
                        pong[1] = (unsigned char)payload_len;
                        memcpy(pong + 2, buf + pos + header_len, payload_len);
                        SSL_write(ssl, pong, 2 + payload_len);
                        std::cout << "WebSocket Ping Frame, auto reply Pong" << std::endl;
                    }
                    else if (payload_len <= 0xFFFF)
                    {
                        pong[1] = 126;
                        pong[2] = (payload_len >> 8) & 0xFF;
                        pong[3] = payload_len & 0xFF;
                        memcpy(pong + 4, buf + pos + header_len, payload_len);
                        SSL_write(ssl, pong, 4 + payload_len);
                        std::cout << "WebSocket Ping Frame, auto reply Pong" << std::endl;
                    }
                    else
                    {
                        pong[1] = 127;
                        for (int i = 0; i < 8; ++i)
                            pong[2 + i] = (payload_len >> (56 - i * 8)) & 0xFF;
                        memcpy(pong + 10, buf + pos + header_len, payload_len);
                        SSL_write(ssl, pong, 10 + payload_len);
                        std::cout << "WebSocket Ping Frame, auto reply Pong" << std::endl;
                    }
                }
            }
            // pong帧，忽略
            else if (opcode == 0xA)
            {
                // std::cout << "WebSocket Pong Frame" << std::endl;
            }
            // close帧，打印并可选关闭
            else if (opcode == 0x8)
            {
                std::cout << "WebSocket Close Frame" << std::endl;
                // 可选：关闭连接
            }

            pos += header_len + payload_len;
        }
        if (pos != start)
            start = pos;
        if (start == end)
            start = end = 0;
    }
};

int main()
{
    std::string host = "stream.binance.com";
    std::string port = "9443";
    std::string path = "/ws/btcusdt@trade";

    // 初始化 OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx)
    {
        std::cerr << "SSL_CTX_new failed\n";
        return 1;
    }

    // 域名解析
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
    {
        std::cerr << "getaddrinfo failed\n";
        SSL_CTX_free(ctx);
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        std::cerr << "socket failed\n";
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        std::cerr << "connect failed\n";
        close(sock);
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        return 1;
    }
    freeaddrinfo(res);

    // 建立 SSL 连接
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0)
    {
        std::cerr << "SSL_connect failed\n";
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return 1;
    }

    // 发送 HTTP 升级请求
    std::string req = build_ws_handshake(host, path);
    SSL_write(ssl, req.c_str(), req.size());

    // 读取并打印响应头
    char buf[4096];
    int n = SSL_read(ssl, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        std::cerr << "SSL_read failed\n";
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return 1;
    }
    buf[n] = 0;
    std::cout << "Handshake response:\n"
              << buf << std::endl;

    // 极致性能循环缓冲区处理 WebSocket 数据帧
    WSBuffer wsbuf;
    wsbuf.set_ssl(ssl); // 让 wsbuf 能自动回复 pong
    while (true)
    {
        int rn = SSL_read(ssl, buf, sizeof(buf));
        if (rn <= 0)
            break;
        wsbuf.append(buf, rn);
        wsbuf.parse_and_print();
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return 0;
}