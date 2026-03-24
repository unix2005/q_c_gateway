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

#ifdef __APPLE__
#include <sys/event.h>
#else
#include <sys/epoll.h>
#endif

#include "../include/server.h"
#include "../include/config.h"
#include "../include/log.h"
#include "../include/service_registry.h"
#include "../include/load_balancer.h"
#include "../include/rate_limit.h"
#include "../include/auth.h"

#define MAX_EVENTS 1024

#ifdef __APPLE__
typedef int epoll_fd_t;
typedef struct kevent epoll_event_t;
#else
typedef int epoll_fd_t;
typedef struct epoll_event epoll_event_t;
#endif

typedef struct {
    epoll_fd_t epoll_fd;
    int listen_fd;
    Config *config;
} ThreadData;

static int listen_fd = -1;
static pthread_t *threads = NULL;
static int thread_count = 0;
static pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;
static int stop_flag = 0;

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2

#ifdef __APPLE__
static int epoll_create1(int flags) {
    return kqueue();
}

static int epoll_ctl(epoll_fd_t epoll_fd, int op, int fd, epoll_event_t *event) {
    struct kevent kev;
    switch (op) {
        case EPOLL_CTL_ADD:
            EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
            return kevent(epoll_fd, &kev, 1, NULL, 0, NULL);
        case EPOLL_CTL_DEL:
            EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            return kevent(epoll_fd, &kev, 1, NULL, 0, NULL);
        default:
            return -1;
    }
}

static int epoll_wait(epoll_fd_t epoll_fd, epoll_event_t *events, int maxevents, int timeout) {
    struct timespec ts;
    if (timeout == -1) {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    } else {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000;
    }
    return kevent(epoll_fd, NULL, 0, events, maxevents, &ts);
}

#define EPOLLIN EVFILT_READ
#define EPOLLET EV_CLEAR
#else
#define EPOLLIN EPOLLIN
#define EPOLLET EPOLLET
#endif

static void *thread_func(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    epoll_fd_t epoll_fd = data->epoll_fd;
    int listen_fd = data->listen_fd;
    Config *config = data->config;

    epoll_event_t events[MAX_EVENTS];

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            log_error("epoll_wait failed: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
#ifdef __APPLE__
            int fd = events[i].ident;
#else
            int fd = events[i].data.fd;
#endif
            if (fd == listen_fd) {
                // 处理新连接
                struct sockaddr_storage client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    log_error("accept failed: %s", strerror(errno));
                    continue;
                }

                // 设置非阻塞
                if (set_nonblocking(client_fd) == -1) {
                    log_error("set_nonblocking failed: %s", strerror(errno));
                    close(client_fd);
                    continue;
                }

                // 添加到epoll
                epoll_event_t event;
#ifdef __APPLE__
                EV_SET(&event, client_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
#else
                event.data.fd = client_fd;
                event.events = EPOLLIN | EPOLLET;
#endif
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    log_error("epoll_ctl add failed: %s", strerror(errno));
                    close(client_fd);
                    continue;
                }
            } else {
                // 处理客户端请求
                int client_fd = fd;
#ifdef __APPLE__
                if (events[i].filter == EVFILT_READ) {
#else
                if (events[i].events & EPOLLIN) {
#endif
                    // 读取请求
                    char buffer[4096];
                    int n = read(client_fd, buffer, sizeof(buffer));
                    if (n <= 0) {
                        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            continue;
                        }
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                    } else {
                        // 处理请求
                        // 这里简化处理，实际需要解析HTTP请求
                        // 检查速率限制
                        if (rate_limit_check() != 0) {
                            const char *response = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\n\r\n";
                            write(client_fd, response, strlen(response));
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            continue;
                        }

                        // 检查认证
                        if (auth_check(buffer, n) != 0) {
                            const char *response = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
                            write(client_fd, response, strlen(response));
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            continue;
                        }

                        // 负载均衡
                        char service_name[64];
                        // 这里简化处理，实际需要从请求中提取服务名
                        strcpy(service_name, "user-service");
                        ServiceInstance *instance = load_balancer_select(service_name);
                        if (instance == NULL) {
                            const char *response = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
                            write(client_fd, response, strlen(response));
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            continue;
                        }

                        // 转发请求到后端服务
                        // 这里简化处理，实际需要建立连接并转发
                        const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello World!";
                        write(client_fd, response, strlen(response));
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                    }
                }
            }
        }
    }

    free(data);
    return NULL;
}

int server_start(Config *config) {
    // 创建socket
    if (config->is_ipv6 == 0) {
        // IPv4
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    } else if (config->is_ipv6 == 1) {
        // IPv6
        listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    } else {
        // 双栈
        listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
        int opt = 0;
        setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
    }

    if (listen_fd == -1) {
        log_error("socket failed: %s", strerror(errno));
        return -1;
    }

    // 设置SO_REUSEPORT
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        log_error("setsockopt SO_REUSEPORT failed: %s", strerror(errno));
        close(listen_fd);
        return -1;
    }

    // 绑定地址
    if (config->is_ipv6 == 0) {
        // IPv4
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(config->port);
        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            log_error("bind failed: %s", strerror(errno));
            close(listen_fd);
            return -1;
        }
    } else {
        // IPv6
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(config->port);
        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            log_error("bind failed: %s", strerror(errno));
            close(listen_fd);
            return -1;
        }
    }

    // 监听
    if (listen(listen_fd, SOMAXCONN) == -1) {
        log_error("listen failed: %s", strerror(errno));
        close(listen_fd);
        return -1;
    }

    // 设置非阻塞
    if (set_nonblocking(listen_fd) == -1) {
        log_error("set_nonblocking failed: %s", strerror(errno));
        close(listen_fd);
        return -1;
    }

    // 创建线程池
    thread_count = config->thread_pool_size;
    threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (threads == NULL) {
        log_error("malloc threads failed");
        close(listen_fd);
        return -1;
    }

    // 启动线程
    for (int i = 0; i < thread_count; i++) {
        // 创建epoll
        epoll_fd_t epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            log_error("epoll_create1 failed: %s", strerror(errno));
            close(listen_fd);
            free(threads);
            return -1;
        }

        // 添加监听socket到epoll
        epoll_event_t event;
#ifdef __APPLE__
        EV_SET(&event, listen_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
#else
        event.data.fd = listen_fd;
        event.events = EPOLLIN | EPOLLET;
#endif
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
            log_error("epoll_ctl add listen_fd failed: %s", strerror(errno));
            close(epoll_fd);
            close(listen_fd);
            free(threads);
            return -1;
        }

        // 创建线程数据
        ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
        if (data == NULL) {
            log_error("malloc ThreadData failed");
            close(epoll_fd);
            close(listen_fd);
            free(threads);
            return -1;
        }
        data->epoll_fd = epoll_fd;
        data->listen_fd = listen_fd;
        data->config = config;

        // 启动线程
        if (pthread_create(&threads[i], NULL, thread_func, data) != 0) {
            log_error("pthread_create failed: %s", strerror(errno));
            free(data);
            close(epoll_fd);
            close(listen_fd);
            free(threads);
            return -1;
        }
    }

    log_info("Server started on port %d", config->port);
    return 0;
}

void server_stop() {
    pthread_mutex_lock(&stop_mutex);
    stop_flag = 1;
    pthread_mutex_unlock(&stop_mutex);

    // 关闭监听socket
    if (listen_fd != -1) {
        close(listen_fd);
        listen_fd = -1;
    }

    // 等待线程退出
    if (threads != NULL) {
        for (int i = 0; i < thread_count; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        threads = NULL;
    }

    log_info("Server stopped");
}

void server_wait() {
    // 等待信号
    pause();
}