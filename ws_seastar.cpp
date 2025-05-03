#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>
#include <seastar/core/sleep.hh>
#include <seastar/net/api.hh>
#include <seastar/core/print.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/core/thread.hh>
#include <seastar/util/log.hh>
#include <seastar/net/tls.hh>

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <regex>

using namespace seastar;
using namespace std::chrono_literals;

// Binance WebSocket 连接信息
static constexpr const char *BINANCE_HOST = "stream.binance.com";
static constexpr const uint16_t BINANCE_PORT = 9443;
static constexpr const char *BINANCE_PATH = "/ws/btcusdt@ticker";

// WebSocket 帧类型
enum class WebSocketOpCode : uint8_t
{
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
};

// 处理 WebSocket 消息
void process_message(const std::string &message)
{
    fmt::print("收到消息: {}\n", message);

    // 这里可以添加 JSON 解析逻辑
    // 使用 Seastar 的 JSON 解析或其他库解析行情数据
}

// 解码 WebSocket 帧
std::pair<bool, temporary_buffer<char>> decode_ws_frame(temporary_buffer<char> &buf)
{
    if (buf.size() < 2)
    {
        return {false, temporary_buffer<char>()};
    }

    bool fin = (buf[0] & 0x80) != 0;
    WebSocketOpCode opcode = static_cast<WebSocketOpCode>(buf[0] & 0x0F);
    bool masked = (buf[1] & 0x80) != 0;
    uint64_t payload_len = buf[1] & 0x7F;

    size_t header_len = 2;
    if (payload_len == 126)
    {
        if (buf.size() < 4)
            return {false, temporary_buffer<char>()};
        payload_len = (static_cast<uint16_t>(buf[2]) << 8) | static_cast<uint16_t>(buf[3]);
        header_len = 4;
    }
    else if (payload_len == 127)
    {
        if (buf.size() < 10)
            return {false, temporary_buffer<char>()};
        payload_len = 0;
        for (int i = 0; i < 8; ++i)
        {
            payload_len = (payload_len << 8) | static_cast<uint8_t>(buf[2 + i]);
        }
        header_len = 10;
    }

    uint8_t mask[4] = {0};
    if (masked)
    {
        if (buf.size() < header_len + 4)
            return {false, temporary_buffer<char>()};
        memcpy(mask, buf.get() + header_len, 4);
        header_len += 4;
    }

    if (buf.size() < header_len + payload_len)
    {
        return {false, temporary_buffer<char>()};
    }

    temporary_buffer<char> payload(payload_len);
    memcpy(payload.get_write(), buf.get() + header_len, payload_len);

    // 如果有掩码，解码数据
    if (masked)
    {
        for (size_t i = 0; i < payload_len; ++i)
        {
            payload.get_write()[i] ^= mask[i % 4];
        }
    }

    return {true, std::move(payload)};
}

// 编码 WebSocket 帧
temporary_buffer<char> encode_ws_frame(WebSocketOpCode opcode, const char *data, size_t len, bool fin = true)
{
    size_t header_len = 2;
    if (len > 125 && len <= 65535)
    {
        header_len = 4;
    }
    else if (len > 65535)
    {
        header_len = 10;
    }

    temporary_buffer<char> buf(header_len + len);
    char *ptr = buf.get_write();

    // 设置 FIN 位和操作码
    ptr[0] = static_cast<char>((fin ? 0x80 : 0x00) | static_cast<uint8_t>(opcode));

    // 设置长度
    if (len <= 125)
    {
        ptr[1] = static_cast<char>(len);
    }
    else if (len <= 65535)
    {
        ptr[1] = 126;
        ptr[2] = static_cast<char>((len >> 8) & 0xFF);
        ptr[3] = static_cast<char>(len & 0xFF);
    }
    else
    {
        ptr[1] = 127;
        for (int i = 0; i < 8; ++i)
        {
            ptr[2 + i] = static_cast<char>((len >> (8 * (7 - i))) & 0xFF);
        }
    }

    // 复制数据
    memcpy(ptr + header_len, data, len);

    return buf;
}

// 连接 Binance WebSocket
future<> connect_binance_ws()
{
    return seastar::do_with(
        seastar::tls::credentials_builder(),
        [](auto &creds_builder)
        {
            creds_builder.set_system_trust();
            return creds_builder.build_certificate_credentials().then(
                [](::shared_ptr<seastar::tls::certificate_credentials> creds)
                {
                    return seastar::tls::connect(creds, ipv4_addr(BINANCE_HOST, BINANCE_PORT)).then([](connected_socket s)
                                                                                                    { return do_with(
                                                                                                          std::move(s),
                                                                                                          std::string(),
                                                                                                          false,
                                                                                                          [](auto &s, auto &buffer, auto &handshake_done)
                                                                                                          {
                                                                                                              auto in = s.input();
                                                                                                              auto out = s.output();

                                                                                                              // 构造 WebSocket 握手请求
                                                                                                              std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // 示例 key
                                                                                                              std::string request =
                                                                                                                  fmt::format("GET {} HTTP/1.1\r\n"
                                                                                                                              "Host: {}\r\n"
                                                                                                                              "Upgrade: websocket\r\n"
                                                                                                                              "Connection: Upgrade\r\n"
                                                                                                                              "Sec-WebSocket-Key: {}\r\n"
                                                                                                                              "Sec-WebSocket-Version: 13\r\n\r\n",
                                                                                                                              BINANCE_PATH, BINANCE_HOST, key);

                                                                                                              // 发送握手请求
                                                                                                              return out.write(request).then([&out]
                                                                                                                                             { return out.flush(); })
                                                                                                                  .then([&in, &buffer, &handshake_done]
                                                                                                                        {
                                        // 读取握手响应
                                        return in.read().then([&buffer, &handshake_done](temporary_buffer<char> data) {
                                            buffer.append(data.get(), data.size());
                                            
                                            // 检查握手是否完成
                                            if (buffer.find("\r\n\r\n") != std::string::npos) {
                                                fmt::print("收到握手响应: {}\n", buffer);
                                                
                                                // 检查握手是否成功
                                                if (buffer.find("HTTP/1.1 101") != std::string::npos) {
                                                    fmt::print("WebSocket 握手成功\n");
                                                    handshake_done = true;
                                                } else {
                                                    throw std::runtime_error("WebSocket 握手失败");
                                                }
                                                
                                                buffer.clear();
                                            }
                                            
                                            return make_ready_future<>();
                                        }); })
                                                                                                                  .then([&in, &out, &buffer, &handshake_done]
                                                                                                                        {
                                        // 持续接收消息
                                        return do_until(
                                            [&in] { return in.eof(); },
                                            [&in, &out, &buffer, &handshake_done] {
                                                return in.read().then([&out, &buffer, &handshake_done](temporary_buffer<char> data) {
                                                    if (data.size() == 0) {
                                                        return make_ready_future<>();
                                                    }
                                                    
                                                    buffer.append(data.get(), data.size());
                                                    
                                                    // 解析 WebSocket 帧
                                                    temporary_buffer<char> temp(buffer.data(), buffer.size());
                                                    auto [success, payload] = decode_ws_frame(temp);
                                                    
                                                    if (success) {
                                                        // 处理消息
                                                        std::string message(payload.get(), payload.size());
                                                        process_message(message);
                                                        
                                                        // 清空缓冲区
                                                        buffer.clear();
                                                        
                                                        // 如果是 Ping，回复 Pong
                                                        if ((data.get()[0] & 0x0F) == static_cast<uint8_t>(WebSocketOpCode::Ping)) {
                                                            auto pong = encode_ws_frame(WebSocketOpCode::Pong, payload.get(), payload.size());
                                                            return out.write(std::move(pong)).then([&out] {
                                                                return out.flush();
                                                            });
                                                        }
                                                    }
                                                    
                                                    return make_ready_future<>();
                                                });
                                            }
                                        ); });
                                                                                                          }); });
                });
        });
}

int main(int argc, char **argv)
{
    seastar::app_template app;

    return app.run(argc, argv, []
                   { return connect_binance_ws().then([]
                                                      {
            fmt::print("程序结束\n");
            return make_ready_future<>(); }); });
}