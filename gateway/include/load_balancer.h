#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include "service_registry.h"

int load_balancer_init();
void load_balancer_cleanup();
ServiceInstance *load_balancer_select(const char *service_name);

#endif