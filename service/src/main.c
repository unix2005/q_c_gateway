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
#include "../include/log.h"
#include "../include/employee.h"

int main(int argc, char *argv[]) {
    // 初始化配置
    Config config;
    if (config_init(&config, "config/service.conf") != 0) {
        fprintf(stderr, "Failed to initialize config\n");
        return 1;
    }

    // 初始化日志
    if (log_init(config.log) != 0) {
        fprintf(stderr, "Failed to initialize log\n");
        return 1;
    }

    // 初始化雇员模块
    if (employee_init() != 0) {
        log_error("Failed to initialize employee module");
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
    employee_cleanup();
    log_cleanup();
    config_cleanup(&config);

    return 0;
}