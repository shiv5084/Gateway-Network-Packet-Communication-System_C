# ==============================================================================
# Makefile for Gateway (GW) Network Packet Communication System
# Works seamlessly on both Windows (MinGW) and Linux (GCC)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -Itests

# OS detection
ifeq ($(OS),Windows_NT)
    PLATFORM_LIBS := -lws2_32
    TARGET_EXT := .exe
    RM := del /Q /F
    MKDIR := if not exist build mkdir build
else
    PLATFORM_LIBS := -lpthread
    TARGET_EXT :=
    RM := rm -rf
    MKDIR := mkdir -p build
endif

BUILD_DIR := build
BIN_TESTS := $(BUILD_DIR)/gw_tests$(TARGET_EXT)

# Source files
COMMON_SRCS := src/common/platform.c \
               src/common/logger.c \
               src/common/crc8.c \
               src/common/packet_parser.c

SERVER_SRCS := src/server/validator.c \
               src/server/response_builder.c

CLIENT_SRCS := src/client/request_builder.c \
               src/client/response_parser.c

TEST_SRCS   := tests/unity/unity.c \
               tests/test_platform.c \
               tests/test_parser.c \
               tests/test_validator.c \
               tests/test_runner.c

ALL_SRCS := $(COMMON_SRCS) $(SERVER_SRCS) $(CLIENT_SRCS) $(TEST_SRCS)
ALL_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_SRCS))

.PHONY: all test clean

all: $(BIN_TESTS)

# Object compilation rule
$(BUILD_DIR)/%.o: %.c
	@if not exist $(subst /,\,$(dir $@)) mkdir $(subst /,\,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

# Link test executable
$(BIN_TESTS): $(ALL_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(PLATFORM_LIBS)

test: $(BIN_TESTS)
	@echo "Executing Gateway Unit Tests..."
	$(BIN_TESTS)

clean:
	$(RM) $(BUILD_DIR)
