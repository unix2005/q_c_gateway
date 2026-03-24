CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

SRC_DIR = src
INCLUDE_DIR = include
CONFIG_DIR = config
LOG_DIR = logs

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/config.c $(SRC_DIR)/server.c $(SRC_DIR)/employee.c $(SRC_DIR)/log.c
OBJS = $(SRCS:.c=.o)
TARGET = service

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean