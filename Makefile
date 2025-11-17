CC = gcc
CFLAGS = -Wall -Wextra -g
SRC_DIR = src
BUILD_DIR = build
TARGET = hospital_queue

SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/model/*.c) $(wildcard $(SRC_DIR)/view/*.c) $(wildcard $(SRC_DIR)/controller/*.c) $(wildcard $(SRC_DIR)/util/*.c)
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: prepare $(TARGET)

prepare:
	mkdir -p $(BUILD_DIR)/src
	mkdir -p $(BUILD_DIR)/src/model
	mkdir -p $(BUILD_DIR)/src/view
	mkdir -p $(BUILD_DIR)/src/controller
	mkdir -p $(BUILD_DIR)/src/util

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -I./src -o $(TARGET) $(SRCS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

