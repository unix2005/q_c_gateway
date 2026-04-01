#define _GNU_SOURCE
#include "../include/net_epoll.h"
#include "../include/net_threadpool.h"
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
#include <sys/epoll.h>
#include <netinet/tcp.h>

// Helper: Custom strnstr implementation for portability

typedef enum
{
    NET_IPV4 = 0,
    NET_IPV6 = 1,
    NET_DUALSTACK = 2
} net_stack_type_t;

#define MAX_EVENTS 4096
#define INITIAL_BUF_SIZE 4096
#define MAX_FDS 65536

typedef struct
{
    int client_fd;
    char *data;
    size_t len;
    net_callback_t handler;
    void *arg;
} net_task_data_t;

typedef struct
{
    int fd;
    char *buf;
    size_t capacity;
    size_t length;
    size_t read_pos; // Added: avoid memmove
    char *out_buf;
    size_t out_capacity;
    size_t out_length;
    size_t out_read_pos; // Added: avoid memmove
    pthread_mutex_t send_lock;
    int epoll_fd;
} net_connection_t;

typedef struct
{
    int epoll_fd;
    int listen_fd;
    net_reactor_t *reactor;
} net_thread_data_t;

struct net_reactor
{
    int reactor_count;
    pthread_t *reactor_threads;
    net_threadpool_t *worker_pool;
    net_callback_t handler;
    void *arg;
    uint16_t port;
    net_stack_type_t stack_type;
    bool stop;
    net_connection_t **conns; // FD to Connection mapping
};

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static char *my_strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0')
    {
        len = strlen(find);
        do
        {
            do
            {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}
static int create_listen_socket(uint16_t port, net_stack_type_t stack_type)
{
    int fd;
    int domain = (stack_type == NET_IPV4) ? AF_INET : AF_INET6;

    fd = socket(domain, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    // SO_REUSEPORT
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
    {
        close(fd);
        return -1;
    }

    // Dual-stack support
    if (stack_type == NET_DUALSTACK)
    {
        opt = 0;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == -1)
        {
            close(fd);
            return -1;
        }
    }

    // Bind
    if (domain == AF_INET)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            close(fd);
            return -1;
        }
    }
    else
    {
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(port);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            close(fd);
            return -1;
        }
    }

    if (listen(fd, SOMAXCONN) == -1)
    {
        close(fd);
        return -1;
    }

    if (set_nonblocking(fd) == -1)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static void worker_task_wrapper(void *arg)
{
    net_task_data_t *task_data = (net_task_data_t *)arg;
    task_data->handler(task_data->client_fd, task_data->data, task_data->len, task_data->arg);
    free(task_data->data);
    free(task_data);
}

static void close_connection(int epoll_fd, net_connection_t *conn, net_reactor_t *reactor)
{
    if (reactor && reactor->conns && conn->fd >= 0 && conn->fd < MAX_FDS)
    {
        reactor->conns[conn->fd] = NULL;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);
    if (conn->buf)
        free(conn->buf);
    if (conn->out_buf)
        free(conn->out_buf);
    pthread_mutex_destroy(&conn->send_lock);
    free(conn);
}

static void handle_client_out(int epoll_fd, net_connection_t *conn, net_reactor_t *reactor)
{
    pthread_mutex_lock(&conn->send_lock);
    size_t pending = conn->out_length - conn->out_read_pos;
    if (pending > 0)
    {
        ssize_t n = write(conn->fd, conn->out_buf + conn->out_read_pos, pending);
        if (n > 0)
        {
            conn->out_read_pos += n;
            if (conn->out_read_pos == conn->out_length)
            {
                conn->out_read_pos = 0;
                conn->out_length = 0;
            }
        }
        else if (n < 0 && (errno != EAGAIN && errno != EWOULDBLOCK))
        {
            pthread_mutex_unlock(&conn->send_lock);
            close_connection(epoll_fd, conn, reactor);
            return;
        }
    }

    if (conn->out_length == 0)
    {
        // Fully sent, remove EPOLLOUT to avoid busy loop
        struct epoll_event event;
        event.data.ptr = conn;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &event);
    }
    pthread_mutex_unlock(&conn->send_lock);
}

int net_send(net_reactor_t *reactor, int client_fd, const char *data, size_t len)
{
    if (!reactor || client_fd < 0 || client_fd >= MAX_FDS)
        return -1;

    net_connection_t *conn = reactor->conns[client_fd];
    if (!conn)
        return -1;

    pthread_mutex_lock(&conn->send_lock);

    size_t pending = conn->out_length - conn->out_read_pos;
    // If there's already data in the output buffer, we must append to maintain order
    if (pending > 0)
    {
        goto buffer_it;
    }

    // Try to send directly
    ssize_t n = write(client_fd, data, len);
    if (n >= 0)
    {
        if ((size_t)n < len)
        {
            // Partially sent
            data += n;
            len -= n;
            goto buffer_it;
        }
        pthread_mutex_unlock(&conn->send_lock);
        return 0; // Fully sent
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            pthread_mutex_unlock(&conn->send_lock);
            return -1; // Socket error
        }
        // Would block, must buffer
    }

buffer_it:
    // Optimization: if read_pos is large, compact once instead of many memmoves
    if (conn->out_read_pos > conn->out_capacity / 2)
    {
        memmove(conn->out_buf, conn->out_buf + conn->out_read_pos, pending);
        conn->out_read_pos = 0;
        conn->out_length = pending;
    }

    // Expand out_buf if needed
    if (conn->out_length + len > conn->out_capacity)
    {
        size_t new_cap = conn->out_capacity == 0 ? INITIAL_BUF_SIZE : conn->out_capacity * 2;
        while (new_cap < conn->out_length + len)
            new_cap *= 2;
        char *new_buf = realloc(conn->out_buf, new_cap);
        if (!new_buf)
        {
            pthread_mutex_unlock(&conn->send_lock);
            return -1;
        }
        conn->out_buf = new_buf;
        conn->out_capacity = new_cap;
    }

    memcpy(conn->out_buf + conn->out_length, data, len);
    conn->out_length += len;

    // Enable EPOLLOUT to send the rest later
    struct epoll_event event;
    event.data.ptr = conn;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(conn->epoll_fd, EPOLL_CTL_MOD, client_fd, &event);

    pthread_mutex_unlock(&conn->send_lock);
    return 0;
}

static void handle_client_data(int epoll_fd, net_connection_t *conn, net_reactor_t *reactor)
{
    char tmp[4096];
    while (1)
    {
        ssize_t n = read(conn->fd, tmp, sizeof(tmp));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            close_connection(epoll_fd, conn, reactor);
            return;
        }
        if (n == 0)
        {
            close_connection(epoll_fd, conn, reactor);
            return;
        }

        // Expand buffer if needed
        if (conn->length + n > conn->capacity)
        {
            size_t new_cap = conn->capacity * 2;
            while (new_cap < conn->length + n)
                new_cap *= 2;
            char *new_buf = realloc(conn->buf, new_cap);
            if (!new_buf)
            {
                close_connection(epoll_fd, conn, reactor);
                return;
            }
            conn->buf = new_buf;
            conn->capacity = new_cap;
        }

        memcpy(conn->buf + conn->length, tmp, n);
        conn->length += n;
    }

    // Parse HTTP: Look for \r\n\r\n (End of Headers)
    while (conn->length - conn->read_pos > 0)
    {
        char *current_ptr = conn->buf + conn->read_pos;
        size_t current_len = conn->length - conn->read_pos;

        char *header_end = my_strnstr(current_ptr, "\r\n\r\n", current_len);
        if (!header_end)
            break; // Headers incomplete

        size_t header_len = (header_end - current_ptr) + 4;
        size_t content_length = 0;

        // Simple Content-Length parsing
        char *cl_ptr = strcasestr(current_ptr, "Content-Length:");
        if (cl_ptr && cl_ptr < header_end)
        {
            content_length = (size_t)atoi(cl_ptr + 15);
        }

        size_t total_expected = header_len + content_length;
        if (current_len < total_expected)
            break; // Body incomplete

        // We have a full HTTP request
        if (reactor->worker_pool)
        {
            net_task_data_t *task_data = (net_task_data_t *)malloc(sizeof(net_task_data_t));
            if (task_data)
            {
                task_data->client_fd = conn->fd;
                task_data->len = total_expected;
                task_data->data = (char *)malloc(total_expected);
                if (task_data->data)
                {
                    memcpy(task_data->data, current_ptr, total_expected);
                    task_data->handler = reactor->handler;
                    task_data->arg = reactor->arg;
                    if (net_threadpool_add(reactor->worker_pool, worker_task_wrapper, task_data) == -1)
                    {
                        reactor->handler(conn->fd, task_data->data, total_expected, reactor->arg);
                        free(task_data->data);
                        free(task_data);
                    }
                }
                else
                {
                    free(task_data);
                }
            }
        }
        else
        {
            reactor->handler(conn->fd, current_ptr, total_expected, reactor->arg);
        }

        conn->read_pos += total_expected;
        if (conn->read_pos == conn->length)
        {
            conn->read_pos = 0;
            conn->length = 0;
        }
    }

    // Optimization: compact read buffer if read_pos is large
    if (conn->read_pos > conn->capacity / 2)
    {
        size_t remaining = conn->length - conn->read_pos;
        if (remaining > 0)
        {
            memmove(conn->buf, conn->buf + conn->read_pos, remaining);
        }
        conn->read_pos = 0;
        conn->length = remaining;
    }
}

static void *reactor_thread_func(void *arg)
{
    net_thread_data_t *data = (net_thread_data_t *)arg;
    net_reactor_t *reactor = data->reactor;
    struct epoll_event events[MAX_EVENTS];

    while (!reactor->stop)
    {
        int nfds = epoll_wait(data->epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds == -1)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == data->listen_fd)
            {
                while (!reactor->stop)
                {
                    struct sockaddr_storage client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_fd = accept(data->listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (client_fd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        continue;
                    }

                    set_nonblocking(client_fd);

                    // Set TCP_NODELAY
                    int nodelay = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                    net_connection_t *conn = (net_connection_t *)malloc(sizeof(net_connection_t));
                    if (!conn)
                    {
                        close(client_fd);
                        continue;
                    }
                    memset(conn, 0, sizeof(net_connection_t));
                    conn->fd = client_fd;
                    conn->capacity = INITIAL_BUF_SIZE;
                    conn->buf = (char *)malloc(conn->capacity);
                    conn->length = 0;
                    conn->epoll_fd = data->epoll_fd;
                    pthread_mutex_init(&conn->send_lock, NULL);

                    if (client_fd < MAX_FDS)
                    {
                        reactor->conns[client_fd] = conn;
                    }

                    struct epoll_event event;
                    event.data.ptr = conn;
                    event.events = EPOLLIN | EPOLLET;
                    if (epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
                    {
                        close_connection(data->epoll_fd, conn, reactor);
                        continue;
                    }
                }
            }
            else
            {
                // It's a client FD
                net_connection_t *conn = (net_connection_t *)events[i].data.ptr;
                if (events[i].events & EPOLLIN)
                {
                    handle_client_data(data->epoll_fd, conn, reactor);
                }
                else if (events[i].events & EPOLLOUT)
                {
                    handle_client_out(data->epoll_fd, conn, reactor);
                }
                else if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    close_connection(data->epoll_fd, conn, reactor);
                }
            }
        }
    }

    close(data->listen_fd);
    close(data->epoll_fd);
    free(data);
    return NULL;
}

net_reactor_t *net_reactor_create(int reactor_count, int worker_count, int queue_size)
{
    net_reactor_t *reactor = (net_reactor_t *)malloc(sizeof(net_reactor_t));
    if (!reactor)
        return NULL;

    memset(reactor, 0, sizeof(net_reactor_t));
    reactor->reactor_count = reactor_count;
    reactor->reactor_threads = (pthread_t *)malloc(sizeof(pthread_t) * reactor_count);
    reactor->conns = (net_connection_t **)calloc(MAX_FDS, sizeof(net_connection_t *));

    if (worker_count > 0)
    {
        reactor->worker_pool = net_threadpool_create(worker_count, queue_size);
    }

    return reactor;
}

int net_reactor_run(net_reactor_t *reactor, uint16_t port, net_stack_type_t stack_type,
                    net_callback_t handler, void *arg)
{
    reactor->port = port;
    reactor->stack_type = stack_type;
    reactor->handler = handler;
    reactor->arg = arg;
    reactor->stop = false;

    for (int i = 0; i < reactor->reactor_count; i++)
    {
        int listen_fd = create_listen_socket(port, stack_type);
        if (listen_fd == -1)
            return -1;

        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1)
        {
            close(listen_fd);
            return -1;
        }

        struct epoll_event event;
        event.data.fd = listen_fd;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1)
        {
            close(epoll_fd);
            close(listen_fd);
            return -1;
        }

        net_thread_data_t *data = (net_thread_data_t *)malloc(sizeof(net_thread_data_t));
        if (!data)
        {
            close(epoll_fd);
            close(listen_fd);
            return -1;
        }
        data->epoll_fd = epoll_fd;
        data->listen_fd = listen_fd;
        data->reactor = reactor;

        if (pthread_create(&reactor->reactor_threads[i], NULL, reactor_thread_func, data) != 0)
        {
            free(data);
            close(epoll_fd);
            close(listen_fd);
            return -1;
        }
    }

    return 0;
}

void net_reactor_stop(net_reactor_t *reactor)
{
    if (!reactor)
        return;
    reactor->stop = true;
    for (int i = 0; i < reactor->reactor_count; i++)
    {
        pthread_join(reactor->reactor_threads[i], NULL);
    }
}

void net_reactor_destroy(net_reactor_t *reactor)
{
    if (!reactor)
        return;
    if (reactor->worker_pool)
    {
        net_threadpool_destroy(reactor->worker_pool);
    }
    if (reactor->conns)
    {
        for (int i = 0; i < MAX_FDS; i++)
        {
            if (reactor->conns[i])
            {
                // In a clean shutdown, all connections should be closed by now.
                // But as a fallback, we could close them here if needed.
                close_connection(reactor->conns[i]->epoll_fd, reactor->conns[i], reactor);
            }
        }
        free(reactor->conns);
    }
    free(reactor->reactor_threads);
    free(reactor);
}
