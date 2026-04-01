CC = gcc
CFLAGS = -Wall -Wextra -fPIC -I./include
LDFLAGS = -shared
AR = ar
ARFLAGS = rcs

SRC_DIR = src
OBJ_DIR = obj
LIB_DIR = lib

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

STATIC_LIB = $(LIB_DIR)/libnet_epoll.a
DYNAMIC_LIB = $(LIB_DIR)/libnet_epoll.so

.PHONY: all clean

all: $(STATIC_LIB) $(DYNAMIC_LIB)

$(STATIC_LIB): $(OBJECTS) | $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(DYNAMIC_LIB): $(OBJECTS) | $(LIB_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR)
