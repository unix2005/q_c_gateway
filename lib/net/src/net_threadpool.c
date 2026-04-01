#include "../include/net_threadpool.h"
#include "../include/net_epoll.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void *threadpool_worker(void *arg)
{
    net_threadpool_t *pool = (net_threadpool_t *)arg;

    while (g_exit_flag != 1)
    {
        pthread_mutex_lock(&pool->lock);

        while (pool->count == 0 && !pool->shutdown )
        {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }

        if (pool->shutdown )
        {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }

        net_task_t task = pool->queue[pool->head];
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count--;

        pthread_mutex_unlock(&pool->lock);

        (*(task.function))(task.arg);
    }

    return NULL;
}

net_threadpool_t *net_threadpool_create(int thread_count, int queue_size)
{
    net_threadpool_t *pool = (net_threadpool_t *)malloc(sizeof(net_threadpool_t));
    if (pool == NULL)
        return NULL;

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = false;

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (net_task_t *)malloc(sizeof(net_task_t) * queue_size);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->notify, NULL);

    for (int i = 0; i < thread_count; i++)
    {
        pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void *)pool);
    }

    return pool;
}

int net_threadpool_add(net_threadpool_t *pool, net_threadpool_task_t function, void *arg)
{
    int err = 0;

    pthread_mutex_lock(&pool->lock);

    if (pool->count == pool->queue_size)
    {
        err = -1; // Queue full
    }
    else if (pool->shutdown)
    {
        err = -1; // Shutdown
    }
    else
    {
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].arg = arg;
        pool->tail = (pool->tail + 1) % pool->queue_size;
        pool->count++;
        pthread_cond_signal(&pool->notify);
    }

    pthread_mutex_unlock(&pool->lock);

    return err;
}

void net_threadpool_destroy(net_threadpool_t *pool)
{
    if (pool == NULL)
        return;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->thread_count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);
    free(pool->queue);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    free(pool);
}
