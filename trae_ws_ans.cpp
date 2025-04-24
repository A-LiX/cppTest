#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

#include "ans_ip_config.h"
#include "ans_errno.h"
#include "dpdk_module.h"
#include "ans_init.h"

namespace beast = boost::beast;

#define MAX_EVENTS 512
#define MAX_BUFFER_SIZE 4096

struct websocket_client {
    int sockfd;
    struct sockaddr_in server_addr;
    beast::flat_buffer buffer;  // 使用 flat_buffer 替代原始字符数组
    SSL_CTX *ssl_ctx;
    SSL *ssl;
};

static void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

static void cleanup_openssl() {
    EVP_cleanup();
}

static SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        printf("Failed to create SSL context\n");
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    return ctx;
}

static int init_dpdk_ans() {
    int ret;
    struct ans_init_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.mbuf_pool_size = 1024;
    cfg.tcp_max_conn = 1024;
    
    ret = ans_initialize(&cfg);
    if (ret != 0) {
        printf("Failed to initialize DPDK-ANS\n");
        return -1;
    }
    return 0;
}

static int create_websocket_client(struct websocket_client *client) {
    // 初始化 OpenSSL
    init_openssl();
    client->ssl_ctx = create_ssl_context();
    if (!client->ssl_ctx) {
        return -1;
    }

    client->sockfd = ans_socket(AF_INET, SOCK_STREAM, 0);
    if (client->sockfd < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(443);
    inet_pton(AF_INET, "stream.binance.com", &client->server_addr.sin_addr);

    // 创建 SSL 对象
    client->ssl = SSL_new(client->ssl_ctx);
    if (!client->ssl) {
        printf("Failed to create SSL object\n");
        return -1;
    }

    return 0;
}

static int perform_tls_handshake(struct websocket_client *client) {
    // 设置 SSL 套接字
    SSL_set_fd(client->ssl, client->sockfd);

    // 执行 TLS 握手
    if (SSL_connect(client->ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        printf("Failed to perform TLS handshake\n");
        return -1;
    }

    printf("TLS handshake successful\n");
    printf("Using cipher: %s\n", SSL_get_cipher(client->ssl));
    return 0;
}

static int connect_to_server(struct websocket_client *client) {
    if (ans_connect(client->sockfd, (struct sockaddr *)&client->server_addr, 
                    sizeof(client->server_addr)) < 0) {
        printf("Failed to connect to server\n");
        return -1;
    }
    return 0;
}

static int send_websocket_handshake(struct websocket_client *client) {
    const char *handshake = 
        "GET /ws/btcusdt@trade HTTP/1.1\r\n"
        "Host: stream.binance.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    // 使用 SSL_write 替代 ans_write
    if (SSL_write(client->ssl, handshake, strlen(handshake)) < 0) {
        printf("Failed to send WebSocket handshake\n");
        return -1;
    }
    return 0;
}

static int handle_websocket_frame(struct websocket_client *client, const char* data, size_t length) {
    // 处理 WebSocket 帧的逻辑
    printf("Processing WebSocket frame, length: %zu\n", length);
    printf("Frame data: %.*s\n", (int)length, data);
    return 0;
}

static int process_received_data(struct websocket_client *client, const char* data, size_t length) {
    // 将接收到的数据添加到 flat_buffer
    auto buffer = net::buffer(data, length);
    client->buffer.commit(net::buffer_copy(client->buffer.prepare(length), buffer));

    // 处理完整的 WebSocket 帧
    while (client->buffer.size() >= 2) {  // 最小帧大小为2字节
        const uint8_t* data = static_cast<const uint8_t*>(client->buffer.data().data());
        bool fin = (data[0] & 0x80) != 0;
        uint8_t opcode = data[0] & 0x0F;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t payload_length = data[1] & 0x7F;
        size_t header_length = 2;

        // 处理扩展长度
        if (payload_length == 126) {
            if (client->buffer.size() < 4) return 0;  // 需要更多数据
            payload_length = (data[2] << 8) | data[3];
            header_length += 2;
        } else if (payload_length == 127) {
            if (client->buffer.size() < 10) return 0;  // 需要更多数据
            payload_length = 0;
            for (int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | data[2 + i];
            }
            header_length += 8;
        }

        // 处理掩码
        if (masked) {
            header_length += 4;
        }

        size_t frame_length = header_length + payload_length;
        if (client->buffer.size() < frame_length) {
            return 0;  // 需要更多数据
        }

        // 提取和处理有效载荷
        const char* payload = static_cast<const char*>(client->buffer.data().data()) + header_length;
        if (masked) {
            // 解码掩码数据
            const uint8_t* mask = data + header_length - 4;
            char* unmasked = new char[payload_length];
            for (size_t i = 0; i < payload_length; ++i) {
                unmasked[i] = payload[i] ^ mask[i % 4];
            }
            handle_websocket_frame(client, unmasked, payload_length);
            delete[] unmasked;
        } else {
            handle_websocket_frame(client, payload, payload_length);
        }

        // 消费已处理的数据
        client->buffer.consume(frame_length);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct websocket_client client;
    int ret;

    // 初始化 DPDK-ANS
    ret = init_dpdk_ans();
    if (ret != 0) {
        return -1;
    }

    // 创建 WebSocket 客户端
    ret = create_websocket_client(&client);
    if (ret != 0) {
        return -1;
    }

    // 连接到服务器
    ret = connect_to_server(&client);
    if (ret != 0) {
        ans_close(client.sockfd);
        return -1;
    }

    // 连接到服务器后执行 TLS 握手
    ret = perform_tls_handshake(&client);
    if (ret != 0) {
        SSL_free(client.ssl);
        SSL_CTX_free(client.ssl_ctx);
        ans_close(client.sockfd);
        return -1;
    }

    // 发送 WebSocket 握手请求
    ret = send_websocket_handshake(&client);
    if (ret != 0) {
        SSL_free(client.ssl);
        SSL_CTX_free(client.ssl_ctx);
        ans_close(client.sockfd);
        return -1;
    }

    // 主事件循环
    while (1) {
        char temp_buffer[MAX_BUFFER_SIZE];
        ssize_t n = SSL_read(client.ssl, temp_buffer, MAX_BUFFER_SIZE);
        if (n < 0) {
            printf("Failed to read data\n");
            break;
        }
        if (n > 0) {
            process_received_data(&client, temp_buffer, n);
        }
    }

    // 清理资源
    SSL_free(client.ssl);
    SSL_CTX_free(client.ssl_ctx);
    ans_close(client.sockfd);
    cleanup_openssl();
    return 0;
}