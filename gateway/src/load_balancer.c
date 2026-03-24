#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/load_balancer.h"
#include "../include/service_registry.h"
#include "../include/log.h"

#define MAX_SERVICES 100

typedef struct {
    char service_name[MAX_SERVICE_NAME];
    int current_index;
    int current_weight;
    pthread_mutex_t mutex;
} LoadBalancerState;

static LoadBalancerState *states = NULL;
static int state_count = 0;
static int state_capacity = 0;
static pthread_mutex_t states_mutex = PTHREAD_MUTEX_INITIALIZER;

static LoadBalancerState *get_state(const char *service_name) {
    pthread_mutex_lock(&states_mutex);

    for (int i = 0; i < state_count; i++) {
        if (strcmp(states[i].service_name, service_name) == 0) {
            pthread_mutex_unlock(&states_mutex);
            return &states[i];
        }
    }

    // 如果状态不存在，创建新状态
    if (state_count >= state_capacity) {
        pthread_mutex_unlock(&states_mutex);
        return NULL;
    }

    LoadBalancerState *state = &states[state_count];
    strcpy(state->service_name, service_name);
    state->current_index = 0;
    state->current_weight = 0;
    pthread_mutex_init(&state->mutex, NULL);
    state_count++;

    pthread_mutex_unlock(&states_mutex);
    return state;
}

static ServiceInstance *select_round_robin(Service *service) {
    if (service == NULL || service->instance_count == 0) {
        return NULL;
    }

    LoadBalancerState *state = get_state(service->service_name);
    if (state == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&state->mutex);

    // 轮询选择
    ServiceInstance *instance = NULL;
    int start_index = state->current_index;
    do {
        instance = &service->instances[state->current_index];
        if (instance->status == 0) {
            // 找到健康实例
            break;
        }
        state->current_index = (state->current_index + 1) % service->instance_count;
    } while (state->current_index != start_index);

    // 更新当前索引
    state->current_index = (state->current_index + 1) % service->instance_count;

    pthread_mutex_unlock(&state->mutex);

    return instance;
}

static ServiceInstance *select_weighted_round_robin(Service *service) {
    if (service == NULL || service->instance_count == 0) {
        return NULL;
    }

    LoadBalancerState *state = get_state(service->service_name);
    if (state == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&state->mutex);

    // 加权轮询选择
    ServiceInstance *instance = NULL;
    while (1) {
        state->current_index = (state->current_index + 1) % service->instance_count;
        if (state->current_index == 0) {
            state->current_weight -= 1;
            if (state->current_weight <= 0) {
                state->current_weight = 0;
                for (int i = 0; i < service->instance_count; i++) {
                    if (service->instances[i].status == 0) {
                        state->current_weight += service->instances[i].weight;
                    }
                }
                if (state->current_weight <= 0) {
                    // 没有健康实例
                    pthread_mutex_unlock(&state->mutex);
                    return NULL;
                }
            }
        }

        if (service->instances[state->current_index].status == 0 && 
            service->instances[state->current_index].weight >= state->current_weight) {
            // 找到合适的实例
            instance = &service->instances[state->current_index];
            break;
        }
    }

    pthread_mutex_unlock(&state->mutex);
    return instance;
}

static ServiceInstance *select_least_connections(Service *service) {
    if (service == NULL || service->instance_count == 0) {
        return NULL;
    }

    // 选择连接数最少的健康实例
    ServiceInstance *best_instance = NULL;
    int min_connections = -1;

    for (int i = 0; i < service->instance_count; i++) {
        ServiceInstance *instance = &service->instances[i];
        if (instance->status == 0) {
            if (min_connections == -1 || instance->connections < min_connections) {
                min_connections = instance->connections;
                best_instance = instance;
            }
        }
    }

    return best_instance;
}

int load_balancer_init() {
    state_capacity = MAX_SERVICES;
    states = (LoadBalancerState *)malloc(sizeof(LoadBalancerState) * state_capacity);
    if (states == NULL) {
        log_error("malloc states failed");
        return -1;
    }

    state_count = 0;
    log_info("Load balancer initialized");
    return 0;
}

void load_balancer_cleanup() {
    if (states != NULL) {
        for (int i = 0; i < state_count; i++) {
            pthread_mutex_destroy(&states[i].mutex);
        }
        free(states);
        states = NULL;
    }
    state_count = 0;
    state_capacity = 0;
    log_info("Load balancer cleaned up");
}

ServiceInstance *load_balancer_select(const char *service_name) {
    // 获取服务
    Service *service = service_registry_get(service_name);
    if (service == NULL) {
        log_error("Service %s not found", service_name);
        return NULL;
    }

    // 加锁服务
    pthread_mutex_lock(&service->mutex);

    // 选择负载均衡策略
    // 这里简化处理，使用轮询策略
    ServiceInstance *instance = select_round_robin(service);

    // 更新连接数
    if (instance != NULL) {
        instance->connections++;
    }

    pthread_mutex_unlock(&service->mutex);

    return instance;
}