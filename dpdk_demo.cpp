#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;

// DPDK 初始化与设置
void dpdk_init(int argc, char* argv[]) {
    if (rte_eal_init(argc, argv) < 0) {
        std::cerr << "[DPDK] EAL init failed!" << std::endl;
        exit(1);
    }
    std::cout << "[DPDK] Initialized!" << std::endl;
}

// 获取 Binance 数据并处理
void dpdk_run(int argc, char* argv[]) {
    dpdk_init(argc, argv);
    
    // DPDK 网络端口配置
    uint8_t port_id = 0;
    rte_eth_dev_configure(port_id, 1, 1, nullptr);

    // 设置 WebSocket 连接
    asio::io_context ioc;
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve("stream.binance.com", "9443");

    beast::tcp_stream stream{ioc};
    stream.connect(results);

    websocket::stream<beast::tcp_stream> ws{std::move(stream)};
    ws.handshake("stream.binance.com", "/ws/btcusdt@depth");

    std::cout << "[WebSocket] Connected to Binance." << std::endl;

    // 接收数据包并解析 WebSocket 数据
    while (true) {
        struct rte_mbuf *bufs[32];
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, 32);

        for (int i = 0; i < nb_rx; ++i) {
            // 从 DPDK 接收到的数据包
            std::string packet_data(reinterpret_cast<char*>(bufs[i]->buf_addr), bufs[i]->data_len);

            // 打印接收到的 Binance 行情数据
            std::cout << "[DPDK] Received Data: " << packet_data << std::endl;

            rte_pktmbuf_free(bufs[i]);
        }

        rte_delay_us_block(1000); // 延时模拟
    }
}

int main(int argc, char* argv[]) {
    // 启动 DPDK 线程
    std::thread dpdk_thread([]() {
        dpdk_run(argc, argv);
    });

    dpdk_thread.join();
    return 0;
}