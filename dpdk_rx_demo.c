#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

// 定义接收队列长度
#define RX_RING_SIZE 128
// 定义 mbuf 池的数量
#define NUM_MBUFS 8191
// mbuf 缓存大小
#define MBUF_CACHE_SIZE 250
// 每次最多收包数量
#define BURST_SIZE 32
#define NUM_RX_QUEUES 2

int main(int argc, char *argv[]) {
    // 初始化 DPDK 环境抽象层（EAL）
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL init\n");

    uint16_t port_id = 0; // 使用第一个网卡端口
    // 创建 mbuf 内存池，用于存放数据包
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // 配置以太网设备
    struct rte_eth_conf port_conf = {0};
    // 配置以太网设备，2个RX队列，1个TX队列
    ret = rte_eth_dev_configure(port_id, NUM_RX_QUEUES, 1, &port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, port_id);

    // 分别设置2个 RX 队列
    for (int q = 0; q < NUM_RX_QUEUES; ++q) {
        ret = rte_eth_rx_queue_setup(port_id, q, RX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u, queue=%d\n", ret, port_id, q);
    }

    // 启动以太网设备
    ret = rte_eth_dev_start(port_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n", ret, port_id);

    printf("开始收包...\n");

    struct rte_mbuf *bufs[BURST_SIZE];
    while (1) {
        // 分别从两个队列收包
        for (int q = 0; q < NUM_RX_QUEUES; ++q) {
            uint16_t nb_rx = rte_eth_rx_burst(port_id, q, bufs, BURST_SIZE);
            if (nb_rx > 0) {
                printf("队列%d收到 %u 个包\n", q, nb_rx);
                for (uint16_t i = 0; i < nb_rx; i++) {
                    rte_pktmbuf_free(bufs[i]);
                }
            }
        }
    }

    // 停止并关闭以太网设备（实际不会执行到这里，因为 while(1) 死循环）
    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    return 0;
}