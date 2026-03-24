#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "../include/config.h"
#include "../include/server.h"
#include "../include/service_registry.h"
#include "../include/health_check.h"
#include "../include/load_balancer.h"
#include "../include/rate_limit.h"
#include "../include/auth.h"
#include "../include/log.h"

int main(int argc, char *argv[]) {
    // 初始化配置
    Config config;
    if (config_init(&config, "config/gateway.conf") != 0) {
        fprintf(stderr, "Failed to initialize config\n");
        return 1;
    }

    // 初始化日志
    if (log_init(config.log) != 0) {
        fprintf(stderr, "Failed to initialize log\n");
        return 1;
    }

    // 初始化服务注册表
    if (service_registry_init(config.service_registry_size, config.service_instance_size) != 0) {
        log_error("Failed to initialize service registry");
        return 1;
    }

    // 初始化健康检查
    if (health_check_init(config.service_registry_size) != 0) {
        log_error("Failed to initialize health check");
        return 1;
    }

    // 初始化负载均衡
    if (load_balancer_init() != 0) {
        log_error("Failed to initialize load balancer");
        return 1;
    }

    // 初始化速率限制
    if (rate_limit_init(config.rate_limit_max_tokens, config.rate_limit_refill_rate) != 0) {
        log_error("Failed to initialize rate limit");
        return 1;
    }

    // 初始化认证授权
    if (auth_init() != 0) {
        log_error("Failed to initialize auth");
        return 1;
    }

    // 启动服务器
    if (server_start(&config) != 0) {
        log_error("Failed to start server");
        return 1;
    }

    // 等待服务器停止
    server_wait();

    // 清理资源
    server_stop();
    auth_cleanup();
    rate_limit_cleanup();
    load_balancer_cleanup();
    health_check_cleanup();
    service_registry_cleanup();
    log_cleanup();
    config_cleanup(&config);

    return 0;
}