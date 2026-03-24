#ifndef SERVICE_REGISTRY_H
#define SERVICE_REGISTRY_H

#define MAX_SERVICE_NAME 64
#define MAX_SERVICE_INSTANCES 32

typedef struct {
    char service_name[MAX_SERVICE_NAME];
    char ip[32];
    int port;
    int status; // 0: 健康, 1: 不健康
    int weight; // 权重
    int connections; // 当前连接数
} ServiceInstance;

typedef struct {
    char service_name[MAX_SERVICE_NAME];
    ServiceInstance *instances;
    int instance_count;
    int instance_capacity;
    pthread_mutex_t mutex;
} Service;

int service_registry_init(int service_size, int instance_size);
void service_registry_cleanup();
int service_registry_register(const char *service_name, const char *ip, int port, int weight);
int service_registry_unregister(const char *service_name, const char *ip, int port);
Service *service_registry_get(const char *service_name);

#endif