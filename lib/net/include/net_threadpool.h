#ifndef NET_THREADPOOL_H
#define NET_THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

typedef void (*net_threadpool_task_t)(void *arg);

typedef struct {
    net_threadpool_task_t function;
    void *arg;
} net_task_t;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    net_task_t *queue;
    int thread_count;
    int queue_size;
    int head;
    int tail;
    int count;
    bool shutdown;
} net_threadpool_t;

net_threadpool_t* net_threadpool_create(int thread_count, int queue_size);
int net_threadpool_add(net_threadpool_t *pool, net_threadpool_task_t function, void *arg);
void net_threadpool_destroy(net_threadpool_t *pool);

#endif // NET_THREADPOOL_H
