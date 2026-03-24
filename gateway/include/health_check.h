#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

int health_check_init(int service_size);
void health_check_cleanup();
void health_check_start();
void health_check_stop();

#endif