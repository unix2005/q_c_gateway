#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "../include/rate_limit.h"
#include "../include/log.h"

typedef struct {
    int max_tokens;
    int current_tokens;
    int refill_rate; // 每秒填充的令牌数
    time_t last_refill_time;
    pthread_mutex_t mutex;
} TokenBucket;

static TokenBucket *bucket = NULL;

static void refill_tokens() {
    time_t now = time(NULL);
    int elapsed = now - bucket->last_refill_time;
    if (elapsed > 0) {
        int tokens_to_add = elapsed * bucket->refill_rate;
        bucket->current_tokens = bucket->current_tokens + tokens_to_add;
        if (bucket->current_tokens > bucket->max_tokens) {
            bucket->current_tokens = bucket->max_tokens;
        }
        bucket->last_refill_time = now;
    }
}

int rate_limit_init(int max_tokens, int refill_rate) {
    bucket = (TokenBucket *)malloc(sizeof(TokenBucket));
    if (bucket == NULL) {
        log_error("malloc bucket failed");
        return -1;
    }

    bucket->max_tokens = max_tokens;
    bucket->current_tokens = max_tokens;
    bucket->refill_rate = refill_rate;
    bucket->last_refill_time = time(NULL);
    pthread_mutex_init(&bucket->mutex, NULL);

    log_info("Rate limit initialized with max tokens %d, refill rate %d per second", max_tokens, refill_rate);
    return 0;
}

void rate_limit_cleanup() {
    if (bucket != NULL) {
        pthread_mutex_destroy(&bucket->mutex);
        free(bucket);
        bucket = NULL;
    }
    log_info("Rate limit cleaned up");
}

int rate_limit_check() {
    if (bucket == NULL) {
        return 0; // 未初始化，允许请求
    }

    pthread_mutex_lock(&bucket->mutex);

    // 填充令牌
    refill_tokens();

    // 检查令牌是否足够
    if (bucket->current_tokens > 0) {
        bucket->current_tokens--;
        pthread_mutex_unlock(&bucket->mutex);
        return 0; // 允许请求
    }

    pthread_mutex_unlock(&bucket->mutex);
    return -1; // 拒绝请求
}