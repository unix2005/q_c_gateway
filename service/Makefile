CC = gcc
CFLAGS = -Wall -Wextra -g -I$(INCLUDE_DIR) -I../lib/net/include
LDFLAGS = -pthread -L../lib/net/lib -lnet_epoll

SRC_DIR = src
INCLUDE_DIR = include
CONFIG_DIR = config
LOG_DIR = logs

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/config.c $(SRC_DIR)/server.c $(SRC_DIR)/employee.c $(SRC_DIR)/log.c
OBJS = $(SRCS:.c=.o)
TARGET = service

LIBNET = ../lib/net/lib/libnet_epoll.a

all: $(LIBNET) $(TARGET)

$(LIBNET):
	$(MAKE) -C ../lib/net

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	$(MAKE) -C ../lib/net clean
