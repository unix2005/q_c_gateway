#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "../include/server.h"
#include "../include/config.h"
#include "../include/log.h"
#include "../include/employee.h"
#include "../../lib/net/include/net_epoll.h"

#define MAX_BUFFER 4096

static net_reactor_t *reactor = NULL;

static void handle_employee_request(int client_fd, const char *method, const char *path, const char *body, int body_len) {
    if (strcmp(method, "GET") == 0) {
        if (strstr(path, "/employee/photo/") != NULL) {
            // 处理照片查询
            char emp_id[64];
            sscanf(path, "/employee/photo/%s", emp_id);
            employee_photo_get(client_fd, emp_id);
        } else if (strstr(path, "/employee/") != NULL) {
            // 处理基本信息查询
            char emp_id[64];
            sscanf(path, "/employee/%s", emp_id);
            employee_info_get(client_fd, emp_id);
        } else {
            // 处理所有雇员信息查询
            employee_list_get(client_fd);
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strstr(path, "/employee/photo/") != NULL) {
            // 处理照片上传
            char emp_id[64];
            sscanf(path, "/employee/photo/%s", emp_id);
            employee_photo_post(client_fd, emp_id, body, body_len);
        } else if (strcmp(path, "/employee") == 0) {
            // 处理雇员信息创建
            employee_info_post(client_fd, body, body_len);
        } else {
            // 处理其他POST请求
            const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            write(client_fd, response, strlen(response));
        }
    } else {
        // 处理其他HTTP方法
        const char *response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, response, strlen(response));
    }
}

static void net_handler(int client_fd, const char *data, size_t len, void *arg) {
    (void)arg;
    if (len > 0) {
        char method[16], path[256], body[MAX_BUFFER];
        int body_len;
        parse_http_request(data, (int)len, method, path, body, &body_len);
        handle_employee_request(client_fd, method, path, body, body_len);
    }
}

static void parse_http_request(const char *request, int length, char *method, char *path, char *body, int *body_len) {
    // 解析HTTP请求
    sscanf(request, "%s %s", method, path);

    // 查找请求体
    const char *body_start = strstr(request, "\r\n\r\n");
    if (body_start != NULL) {
        body_start += 4;
        *body_len = length - (body_start - request);
        if (*body_len > 0) {
            strncpy(body, body_start, *body_len);
            body[*body_len] = '\0';
        }
    } else {
        *body_len = 0;
        body[0] = '\0';
    }
}

int server_start(Config *config) {
    int reactor_count = config->thread_pool_size; // Use configured threads for reactors
    int worker_count = config->thread_pool_size;  // Use same for worker pool

    reactor = net_reactor_create(reactor_count, worker_count);
    if (!reactor) {
        log_error("Failed to create net_reactor");
        return -1;
    }

    net_stack_type_t stack_type;
    if (config->is_ipv6 == 0) {
        stack_type = NET_IPV4;
    } else if (config->is_ipv6 == 1) {
        stack_type = NET_IPV6;
    } else {
        stack_type = NET_DUALSTACK;
    }

    if (net_reactor_run(reactor, (uint16_t)config->service_port, stack_type, net_handler, NULL) == -1) {
        log_error("Failed to run net_reactor");
        net_reactor_destroy(reactor);
        reactor = NULL;
        return -1;
    }

    log_info("Server started on port %d with %d reactors", config->service_port, reactor_count);
    return 0;
}

void server_stop() {
    if (reactor) {
        net_reactor_stop(reactor);
        net_reactor_destroy(reactor);
        reactor = NULL;
    }
    log_info("Server stopped");
}

void server_wait() {
    // 等待信号
    pause();
}