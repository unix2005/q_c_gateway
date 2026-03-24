#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATH 256

typedef struct {
    int port;
    int thread_pool_size;
    int thread_queue_size;
    int max_connections;
    int max_cache_items;
    int max_cache_size;
    int rate_limit_max_tokens;
    int rate_limit_refill_rate;
    int service_registry_size;
    int service_instance_size;
    char tls_cert_file[MAX_PATH];
    char tls_key_file[MAX_PATH];
    int is_ipv6;
    int is_https;
    char log[MAX_PATH];
} Config;

int config_init(Config *config, const char *config_file);
void config_cleanup(Config *config);

#endif