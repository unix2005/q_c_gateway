#ifndef NET_EPOLL_H
#define NET_EPOLL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct net_reactor net_reactor_t;

// Callback for processing client requests
// client_fd: socket for the client connection
// data: pointer to the complete packet data (excluding header)
// len: length of the packet data
// arg: user-provided argument
typedef void (*net_callback_t)(int client_fd, const char *data, size_t len, void *arg);

/**
 * Create a multi-reactor system.
 * @param reactor_count Number of reactor threads (usually matching CPU cores).
 * @param worker_count Number of worker threads in the pool for task processing.
 * @return Pointer to net_reactor_t or NULL on failure.
 */
net_reactor_t* net_reactor_create(int reactor_count, int worker_count,int queue_size);

/**
 * Start listening on a port and run the event loops.
 * @param reactor Pointer to net_reactor_t.
 * @param port Port to listen on.
 * @param stack_type IP stack type (IPv4, IPv6, or Dual).
 * @param handler Callback function to handle client requests.
 * @param arg User argument passed to the handler.
 * @return 0 on success, -1 on failure.
 */
int net_reactor_run(net_reactor_t *reactor, uint16_t port, net_stack_type_t stack_type, 
                    net_callback_t handler, void *arg);

/**
 * Stop the reactor system.
 */
void net_reactor_stop(net_reactor_t *reactor);

/**
 * Destroy and free reactor resources.
 */
void net_reactor_destroy(net_reactor_t *reactor);

/**
 * Non-blocking send data to a client.
 * If kernel buffer is full, data will be buffered in application layer.
 * @param reactor Pointer to net_reactor_t.
 * @param client_fd Socket for the client.
 * @param data Pointer to the data to send.
 * @param len Length of the data.
 * @return 0 on success (fully sent or buffered), -1 on error.
 */
int net_send(net_reactor_t *reactor, int client_fd, const char *data, size_t len);

#endif // NET_EPOLL_H
