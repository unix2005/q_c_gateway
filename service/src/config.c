#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/config.h"

int config_init(Config *config, const char *config_file) {
    // 设置默认值
    strcpy(config->service_name, "user-service");
    config->service_port = 8081;
    strcpy(config->gateway_url, "http://localhost:8080");
    config->heartbeat_interval = 30;
    config->thread_pool_size = 4;
    config->thread_queue_size = 100;
    strcpy(config->tls_cert_file, "cert.pem");
    strcpy(config->tls_key_file, "key.pem");
    config->is_ipv6 = 0;
    config->is_https = 0;
    strcpy(config->log, "logs/service");
    config->verify_cert = 0;

    // 读取配置文件
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL) {
        // 配置文件不存在，使用默认值
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        // 解析配置项
        char key[64], value[128];
        if (sscanf(line, "%s %s", key, value) == 2) {
            if (strcmp(key, "service_name") == 0) {
                strcpy(config->service_name, value);
            } else if (strcmp(key, "service_port") == 0) {
                config->service_port = atoi(value);
            } else if (strcmp(key, "gateway_url") == 0) {
                strcpy(config->gateway_url, value);
            } else if (strcmp(key, "heartbeat_interval") == 0) {
                config->heartbeat_interval = atoi(value);
            } else if (strcmp(key, "thread_pool_size") == 0) {
                config->thread_pool_size = atoi(value);
            } else if (strcmp(key, "thread_queue_size") == 0) {
                config->thread_queue_size = atoi(value);
            } else if (strcmp(key, "tls_cert_file") == 0) {
                strcpy(config->tls_cert_file, value);
            } else if (strcmp(key, "tls_key_file") == 0) {
                strcpy(config->tls_key_file, value);
            } else if (strcmp(key, "is_ipv6") == 0) {
                config->is_ipv6 = atoi(value);
            } else if (strcmp(key, "is_https") == 0) {
                config->is_https = atoi(value);
            } else if (strcmp(key, "log") == 0) {
                strcpy(config->log, value);
            } else if (strcmp(key, "verify_cert") == 0) {
                config->verify_cert = atoi(value);
            }
        }
    }

    fclose(fp);
    return 0;
}

void config_cleanup(Config *config) {
    // 清理配置
    // 由于Config结构体中没有动态分配的内存，所以这里不需要做特别的清理
}