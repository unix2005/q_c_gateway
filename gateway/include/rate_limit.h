#ifndef RATE_LIMIT_H
#define RATE_LIMIT_H

int rate_limit_init(int max_tokens, int refill_rate);
void rate_limit_cleanup();
int rate_limit_check();

#endif