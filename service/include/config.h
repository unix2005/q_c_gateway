#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATH 256

typedef struct {
    char service_name[MAX_PATH];
    int service_port;
    char gateway_url[MAX_PATH];
    int heartbeat_interval;
    int thread_pool_size;
    int thread_queue_size;
    char tls_cert_file[MAX_PATH];
    char tls_key_file[MAX_PATH];
    int is_ipv6;
    int is_https;
    char log[MAX_PATH];
    int verify_cert;
} Config;

int config_init(Config *config, const char *config_file);
void config_cleanup(Config *config);

#endif