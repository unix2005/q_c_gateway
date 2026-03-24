#ifndef AUTH_H
#define AUTH_H

int auth_init();
void auth_cleanup();
int auth_check(const char *request, int length);

#endif