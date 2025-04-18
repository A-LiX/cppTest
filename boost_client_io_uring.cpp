#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>  // 确保包含这个头文件
#include <boost/asio/posix/stream_descriptor.hpp>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <errno.h>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;

#define TRADE_BUFFER_SIZE 256
#define RING_SIZE 256  // io_uring queue size

// io_uring setup
int ring_fd = -1;
struct io_uring ring;

void setup_io_uring()
{
    // 初始化 io_uring
    if (io_uring_queue_init(RING_SIZE, &ring, 0) < 0)
    {
        spdlog::error("io_uring setup failed: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void submit_recv(int fd, void *buf, size_t len)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_recv(sqe, fd, buf, len, 0);
    io_uring_submit(&ring);
}

void wait_for_completion()
{
    struct io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0)
    {
        spdlog::error("io_uring operation failed: {}", strerror(-cqe->res));
        exit(EXIT_FAILURE);
    }
    io_uring_cqe_seen(&ring, cqe);
}

int main()
{
    // 创建 io_context
    net::io_context ioc;

    // 配置日志
    spdlog::info("Starting WebSocket client...");

    // 配置服务器信息
    const std::string host = "stream.binance.com";
    const std::string port = "9443";
    const std::string target = "/ws/btcusdt@trade";

    // 解析 DNS
    net::ip::tcp::resolver resolver(ioc);  // 使用正确的类型
    boost::system::error_code err;
    auto const results = resolver.resolve(host, port, err);
    if (err)
    {
        spdlog::error("DNS resolution failed: {}", err.message());
        return EXIT_FAILURE;
    }

    // 创建 WebSocket 流
    websocket::stream<beast::tcp_stream> ws(ioc);
    net::connect(ws.next_layer(), results.begin(), results.end(), err);
    if (err)
    {
        spdlog::error("TCP connection failed: {}", err.message());
        return EXIT_FAILURE;
    }

    // 执行 WebSocket 握手
    ws.handshake(host, target, err);
    if (err)
    {
        spdlog::error("WebSocket handshake failed: {}", err.message());
        return EXIT_FAILURE;
    }

    spdlog::info("Connected to WebSocket at {}", host);

    // 使用 io_uring 设置文件描述符
    int fd = ws.next_layer().native_handle();
    char buffer[TRADE_BUFFER_SIZE];

    // 设置 io_uring
    setup_io_uring();

    while (true)
    {
        // 提交一次 recv 操作
        submit_recv(fd, buffer, TRADE_BUFFER_SIZE);

        // 等待操作完成
        wait_for_completion();

        // 处理接收到的数据
        std::cout << "Received message: " << std::string(buffer, TRADE_BUFFER_SIZE) << std::endl;
    }

    // 清理 io_uring
    io_uring_queue_exit(&ring);

    return EXIT_SUCCESS;
}