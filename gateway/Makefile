CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

SRC_DIR = src
INCLUDE_DIR = include
CONFIG_DIR = config
LOG_DIR = logs

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/config.c $(SRC_DIR)/server.c $(SRC_DIR)/service_registry.c $(SRC_DIR)/health_check.c $(SRC_DIR)/load_balancer.c $(SRC_DIR)/rate_limit.c $(SRC_DIR)/auth.c $(SRC_DIR)/log.c
OBJS = $(SRCS:.c=.o)
TARGET = gateway

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean