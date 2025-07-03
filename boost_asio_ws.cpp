#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <functional>
#include <deque>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace ssl = net::ssl;
namespace json = boost::json;
using tcp = net::ip::tcp;

// WebSocket 流类型定义
using websocket_t = beast::websocket::stream<beast::ssl_stream<tcp::socket>>;

class GenericWebSocketClient : public std::enable_shared_from_this<GenericWebSocketClient> {
public:
    // 连接配置
    struct Config {
        std::string host;           // 主机地址
        std::string port;           // 端口号
        std::string path;           // WebSocket 路径
        bool use_ssl = true;        // 是否使用 SSL
        bool verbose = true;        // 详细日志输出
        std::string user_agent = "GenericWebSocketClient/1.0"; // 用户代理
        
        // 构造函数
        Config(const std::string& h, const std::string& p, const std::string& pt, 
               bool ssl = true, bool verb = true)
            : host(h), port(p), path(pt), use_ssl(ssl), verbose(verb) {}
    };
    
    // 回调函数类型定义
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using ConnectHandler = std::function<void()>;
    using DisconnectHandler = std::function<void()>;

    GenericWebSocketClient(net::io_context& ioc, const Config& config)
        : resolver_(net::make_strand(ioc)),
          ssl_ctx_(config.use_ssl ? ssl::context::tlsv12_client : ssl::context::tlsv12_client),
          config_(config),
          ws_(std::make_unique<websocket_t>(net::make_strand(ioc), ssl_ctx_))
    {
        // 简化 SSL 配置 - 不加载证书
        if (config_.use_ssl) {
            ssl_ctx_.set_verify_mode(ssl::verify_none); // 不验证证书
        }
    }
    
    ~GenericWebSocketClient() {
        disconnect();
    }
    
    // 设置回调函数
    void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }
    void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }
    void set_connect_handler(ConnectHandler handler) { connect_handler_ = std::move(handler); }
    void set_disconnect_handler(DisconnectHandler handler) { disconnect_handler_ = std::move(handler); }
    
    // 连接 WebSocket
    void connect() {
        if (is_connecting_ || is_connected_) return;
        
        is_connecting_ = true;
        
        if (config_.verbose) {
            log("Connecting to " + config_.host + ":" + config_.port + " | Path: " + config_.path);
        }
        
        // 解析域名
        resolver_.async_resolve(
            config_.host, config_.port,
            beast::bind_front_handler(
                &GenericWebSocketClient::on_resolve,
                shared_from_this()));
    }
    
    // 断开连接
    void disconnect() {
        if (!is_connected_) return;
        
        if (ws_->is_open()) {
            beast::error_code ec;
            ws_->close(beast::websocket::close_code::normal, ec);
            if (ec && config_.verbose) {
                log("Close error: " + ec.message());
            }
        }
        
        is_connected_ = false;
        is_connecting_ = false;
        
        if (disconnect_handler_) {
            disconnect_handler_();
        }
    }
    
    // 发送消息
    void send(const std::string& message) {
        if (!is_connected_) {
            log("Cannot send message - not connected");
            return;
        }
        
        // 将消息添加到发送队列
        send_queue_.push_back(message);
        
        // 如果当前没有发送操作，则开始发送
        if (!is_sending_) {
            do_write();
        }
    }
    
    // 检查连接状态
    bool is_connected() const { return is_connected_; }
    bool is_connecting() const { return is_connecting_; }

private:
    void log(const std::string& message) {
        if (config_.verbose) {
            std::cout << "[WebSocket] " << message << std::endl;
        }
    }
    
    void handle_error(const std::string& message, beast::error_code ec = {}) {
        std::string full_message = message;
        if (ec) {
            full_message += ": " + ec.message();
        }
        
        log(full_message);
        is_connecting_ = false;
        
        if (error_handler_) {
            error_handler_(full_message);
        }
    }
    
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if (ec) {
            handle_error("Resolve failed", ec);
            return;
        }
        
        // 保存端点
        endpoints_ = results;
        
        // 异步连接
        net::async_connect(
            beast::get_lowest_layer(*ws_),
            endpoints_,
            beast::bind_front_handler(
                &GenericWebSocketClient::on_connect,
                shared_from_this()));
    }
    
    void on_connect(beast::error_code ec, const tcp::endpoint&) {
        if (ec) {
            handle_error("Connect failed", ec);
            return;
        }
        
        if (!config_.use_ssl) {
            // 非 SSL 连接直接进行 WebSocket 握手
            on_tcp_connected();
            return;
        }
        
        // 设置 SNI 主机名
        if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), config_.host.c_str())) {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category());
            handle_error("SNI failed", ec);
            return;
        }
        
        // 异步 SSL 握手
        ws_->next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &GenericWebSocketClient::on_ssl_handshake,
                shared_from_this()));
    }
    
    void on_ssl_handshake(beast::error_code ec) {
        if (ec) {
            handle_error("SSL handshake failed", ec);
            return;
        }
        
        if (config_.verbose) {
            log("SSL handshake successful");
        }
        on_tcp_connected();
    }
    
    void on_tcp_connected() {
        // 设置 WebSocket 选项
        ws_->set_option(beast::websocket::stream_base::decorator(
            [this](beast::websocket::request_type& req) {
                req.set(beast::http::field::user_agent, config_.user_agent);
            }));
        
        // 异步 WebSocket 握手
        ws_->async_handshake(
            config_.host, config_.path,
            beast::bind_front_handler(
                &GenericWebSocketClient::on_handshake,
                shared_from_this()));
    }
    
    void on_handshake(beast::error_code ec) {
        if (ec) {
            handle_error("WebSocket handshake failed", ec);
            return;
        }
        
        if (config_.verbose) {
            log("Connected successfully");
        }
        
        is_connected_ = true;
        is_connecting_ = false;
        
        if (connect_handler_) {
            connect_handler_();
        }
        
        // 开始读取消息
        do_read();
    }
    
    void do_read() {
        ws_->async_read(
            buffer_,
            beast::bind_front_handler(
                &GenericWebSocketClient::on_read,
                shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == beast::websocket::error::closed) {
            if (config_.verbose) {
                log("Connection closed by server");
            }
            is_connected_ = false;
            if (disconnect_handler_) {
                disconnect_handler_();
            }
            return;
        }
        
        if (ec) {
            handle_error("Read failed", ec);
            is_connected_ = false;
            return;
        }
        
        // 处理接收到的消息
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(bytes_transferred);
        
        if (message_handler_) {
            message_handler_(message);
        }
        
        // 继续读取下一条消息
        do_read();
    }
    
    void do_write() {
        if (send_queue_.empty()) {
            is_sending_ = false;
            return;
        }
        
        is_sending_ = true;
        const std::string& message = send_queue_.front();
        
        ws_->async_write(
            net::buffer(message),
            beast::bind_front_handler(
                &GenericWebSocketClient::on_write,
                shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            handle_error("Write failed", ec);
            is_sending_ = false;
            return;
        }
        
        // 移除已发送的消息
        send_queue_.pop_front();
        
        // 发送队列中的下一条消息
        do_write();
    }
    
    // 成员变量
    tcp::resolver resolver_;
    ssl::context ssl_ctx_;
    Config config_;
    std::unique_ptr<websocket_t> ws_;
    beast::flat_buffer buffer_;
    tcp::resolver::results_type endpoints_;
    std::deque<std::string> send_queue_;
    bool is_sending_ = false;
    
    // 状态标志
    bool is_connected_ = false;
    bool is_connecting_ = false;
    
    // 回调函数
    MessageHandler message_handler_;
    ErrorHandler error_handler_;
    ConnectHandler connect_handler_;
    DisconnectHandler disconnect_handler_;
};

int main() {
    try {
        net::io_context ioc;
        
        // 示例 1: 连接 Binance 现货交易
        {
            GenericWebSocketClient::Config config(
                "stream.binance.com", 
                "443", 
                "/ws/btcusdt@trade",
                true,  // use_ssl
                true   // verbose
            );
            
            auto client = std::make_shared<GenericWebSocketClient>(ioc, config);
            
            client->set_message_handler([](const std::string& msg) {
                try {
                    auto json_data = json::parse(msg);
                    
                    // 处理交易事件
                    if (json_data.as_object().contains("e") && 
                        json_data.at("e").as_string() == "trade") {
                        std::string symbol = json_data.at("s").as_string().c_str();
                        std::string price = json_data.at("p").as_string().c_str();
                        std::string quantity = json_data.at("q").as_string().c_str();
                        
                        std::cout << "[Binance Trade] " << symbol 
                                << " | Price: " << price
                                << " | Qty: " << quantity << std::endl;
                    }
                    // 其他消息处理...
                } catch (const std::exception& e) {
                    std::cerr << "JSON error: " << e.what() 
                            << "\nMessage: " << msg << std::endl;
                }
            });
            
            client->set_error_handler([](const std::string& error) {
                std::cerr << "Error: " << error << std::endl;
            });
            
            client->set_connect_handler([]() {
                std::cout << "Connected to Binance!" << std::endl;
            });
            
            client->set_disconnect_handler([]() {
                std::cout << "Disconnected from Binance!" << std::endl;
            });
            
            client->connect();
        }
        
        // 示例 2: 连接 Coinbase
        {
            GenericWebSocketClient::Config config(
                "ws-feed.exchange.coinbase.com", 
                "443", 
                "/",
                true,
                true
            );
            
            auto client = std::make_shared<GenericWebSocketClient>(ioc, config);
            
            client->set_message_handler([](const std::string& msg) {
                try {
                    auto json_data = json::parse(msg);
                    
                    if (json_data.as_object().contains("type")) {
                        std::string type = json_data.at("type").as_string().c_str();
                        
                        if (type == "ticker") {
                            std::string product_id = json_data.at("product_id").as_string().c_str();
                            std::string price = json_data.at("price").as_string().c_str();
                            
                            std::cout << "[Coinbase Ticker] " << product_id 
                                    << " | Price: " << price << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "JSON error: " << e.what() 
                            << "\nMessage: " << msg << std::endl;
                }
            });
            
            client->set_error_handler([](const std::string& error) {
                std::cerr << "Error: " << error << std::endl;
            });
            
            client->set_connect_handler([client]() {
                std::cout << "Connected to Coinbase!" << std::endl;
                
                // 发送订阅消息
                const std::string subscribe_msg = R"({
                    "type": "subscribe",
                    "product_ids": ["BTC-USD"],
                    "channels": ["ticker"]
                })";
                
                client->send(subscribe_msg);
            });
            
            client->connect();
        }
        
        // 运行事件循环
        ioc.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}