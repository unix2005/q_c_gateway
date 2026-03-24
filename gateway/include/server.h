#ifndef SERVER_H
#define SERVER_H

#include "config.h"

int server_start(Config *config);
void server_stop();
void server_wait();

#endif