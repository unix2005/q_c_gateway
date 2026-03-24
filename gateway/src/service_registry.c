#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/service_registry.h"
#include "../include/log.h"

static Service *services = NULL;
static int service_count = 0;
static int service_capacity = 0;
static int instance_capacity = 0;
static pthread_mutex_t service_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

int service_registry_init(int service_size, int instance_size) {
    service_capacity = service_size;
    instance_capacity = instance_size;
    services = (Service *)malloc(sizeof(Service) * service_capacity);
    if (services == NULL) {
        log_error("malloc services failed");
        return -1;
    }

    for (int i = 0; i < service_capacity; i++) {
        services[i].service_name[0] = '\0';
        services[i].instances = (ServiceInstance *)malloc(sizeof(ServiceInstance) * instance_capacity);
        if (services[i].instances == NULL) {
            log_error("malloc instances failed");
            for (int j = 0; j < i; j++) {
                free(services[j].instances);
            }
            free(services);
            services = NULL;
            return -1;
        }
        services[i].instance_count = 0;
        services[i].instance_capacity = instance_capacity;
        pthread_mutex_init(&services[i].mutex, NULL);
    }

    log_info("Service registry initialized with capacity %d services, %d instances per service", service_capacity, instance_capacity);
    return 0;
}

void service_registry_cleanup() {
    if (services != NULL) {
        for (int i = 0; i < service_capacity; i++) {
            if (services[i].instances != NULL) {
                free(services[i].instances);
                services[i].instances = NULL;
            }
            pthread_mutex_destroy(&services[i].mutex);
        }
        free(services);
        services = NULL;
    }
    service_count = 0;
    service_capacity = 0;
    instance_capacity = 0;
    log_info("Service registry cleaned up");
}

int service_registry_register(const char *service_name, const char *ip, int port, int weight) {
    pthread_mutex_lock(&service_registry_mutex);

    // 查找服务
    int service_index = -1;
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].service_name, service_name) == 0) {
            service_index = i;
            break;
        }
    }

    // 如果服务不存在，创建新服务
    if (service_index == -1) {
        if (service_count >= service_capacity) {
            pthread_mutex_unlock(&service_registry_mutex);
            log_error("Service registry full");
            return -1;
        }
        service_index = service_count;
        strcpy(services[service_index].service_name, service_name);
        services[service_index].instance_count = 0;
        service_count++;
    }

    pthread_mutex_unlock(&service_registry_mutex);

    // 加锁服务
    pthread_mutex_lock(&services[service_index].mutex);

    // 检查实例是否已存在
    for (int i = 0; i < services[service_index].instance_count; i++) {
        if (strcmp(services[service_index].instances[i].ip, ip) == 0 && services[service_index].instances[i].port == port) {
            // 实例已存在，更新状态
            services[service_index].instances[i].status = 0;
            services[service_index].instances[i].weight = weight;
            pthread_mutex_unlock(&services[service_index].mutex);
            log_info("Service instance %s:%d updated for service %s", ip, port, service_name);
            return 0;
        }
    }

    // 检查实例容量
    if (services[service_index].instance_count >= services[service_index].instance_capacity) {
        pthread_mutex_unlock(&services[service_index].mutex);
        log_error("Service %s instance capacity full", service_name);
        return -1;
    }

    // 添加新实例
    ServiceInstance *instance = &services[service_index].instances[services[service_index].instance_count];
    strcpy(instance->service_name, service_name);
    strcpy(instance->ip, ip);
    instance->port = port;
    instance->status = 0;
    instance->weight = weight;
    instance->connections = 0;
    services[service_index].instance_count++;

    pthread_mutex_unlock(&services[service_index].mutex);
    log_info("Service instance %s:%d registered for service %s", ip, port, service_name);
    return 0;
}

int service_registry_unregister(const char *service_name, const char *ip, int port) {
    pthread_mutex_lock(&service_registry_mutex);

    // 查找服务
    int service_index = -1;
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].service_name, service_name) == 0) {
            service_index = i;
            break;
        }
    }

    if (service_index == -1) {
        pthread_mutex_unlock(&service_registry_mutex);
        log_error("Service %s not found", service_name);
        return -1;
    }

    pthread_mutex_unlock(&service_registry_mutex);

    // 加锁服务
    pthread_mutex_lock(&services[service_index].mutex);

    // 查找实例
    int instance_index = -1;
    for (int i = 0; i < services[service_index].instance_count; i++) {
        if (strcmp(services[service_index].instances[i].ip, ip) == 0 && services[service_index].instances[i].port == port) {
            instance_index = i;
            break;
        }
    }

    if (instance_index == -1) {
        pthread_mutex_unlock(&services[service_index].mutex);
        log_error("Service instance %s:%d not found for service %s", ip, port, service_name);
        return -1;
    }

    // 移除实例
    for (int i = instance_index; i < services[service_index].instance_count - 1; i++) {
        services[service_index].instances[i] = services[service_index].instances[i + 1];
    }
    services[service_index].instance_count--;

    // 如果服务没有实例了，删除服务
    if (services[service_index].instance_count == 0) {
        pthread_mutex_unlock(&services[service_index].mutex);

        pthread_mutex_lock(&service_registry_mutex);
        for (int i = service_index; i < service_count - 1; i++) {
            services[i] = services[i + 1];
        }
        service_count--;
        pthread_mutex_unlock(&service_registry_mutex);

        log_info("Service %s removed (no instances left)", service_name);
    } else {
        pthread_mutex_unlock(&services[service_index].mutex);
    }

    log_info("Service instance %s:%d unregistered for service %s", ip, port, service_name);
    return 0;
}

Service *service_registry_get(const char *service_name) {
    pthread_mutex_lock(&service_registry_mutex);

    // 查找服务
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].service_name, service_name) == 0) {
            pthread_mutex_unlock(&service_registry_mutex);
            return &services[i];
        }
    }

    pthread_mutex_unlock(&service_registry_mutex);
    return NULL;
}