#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "../lib/net/include/net_epoll.h"

// 客户端请求回调函数
// 这个函数会在一个完整的 HTTP 请求（Header + Body）接收完成后执行
void on_client_request(int client_fd, const char *data, size_t len, void *arg) {
    (void)arg;
    
    // 直接使用已经拼凑好的完整数据
    if (len > 0) {
        printf("[Worker] Received full HTTP request (len=%zu):\n%.*s\n", len, (int)len, data);
        
        // 构造 HTTP 响应
        const char *response_body = "Hello from High Performance HTTP Reactor!";
        char response[1024];
        int n = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(response_body), response_body);
        
        write(client_fd, response, n);
    }
}

// 信号处理函数，用于优雅退出
void handle_signal(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n[Main] Stopping server...\n");
        g_exit_flag = 1; // 设置全局退出标志
    }
}

int main(int argc, char *argv[])
{
    uint16_t port = 8080;
    if (argc > 1)
    {
        port = (uint16_t)atoi(argv[1]);
    }

    // 注册信号
    signal(SIGINT, handle_signal);

    // 1. 创建 Reactor 系统
    // 参数1: Reactor 线程数 (通常设为 CPU 核心数)
    // 参数2: 工作线程池大小 (用于处理业务逻辑)
    int reactor_threads = 4;
    int worker_threads = 8;
    int queue_size = 1024;
    net_reactor_t *reactor = net_reactor_create(reactor_threads, worker_threads, queue_size);
    if (!reactor)
    {
        fprintf(stderr, "Failed to create reactor\n");
        return 1;
    }

    printf("[Main] Echo server starting on port %d...\n", port);
    printf("[Main] Multi-Reactor threads: %d, Worker threads: %d, Queue size: %d\n", reactor_threads, worker_threads, queue_size);

    // 2. 启动事件循环
    // 使用 NET_DUALSTACK 同时支持 IPv4 和 IPv6
    if (net_reactor_run(reactor, port, NET_DUALSTACK, on_client_request, NULL) != 0)
    {
        fprintf(stderr, "Failed to run reactor\n");
        net_reactor_destroy(reactor);
        return 1;
    }

    // 3. 主线程等待退出标志
    while (g_exit_flag != 1)
    {
        sleep(1);
    }

    // 4. 清理资源
    net_reactor_stop(reactor);
    net_reactor_destroy(reactor);

    printf("[Main] Server exited cleanly.\n");
    return 0;
}
