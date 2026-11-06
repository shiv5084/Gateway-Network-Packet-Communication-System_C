#!/bin/bash
# ==============================================================================
# Gateway (GW) Network Packet Communication System - Linux Build Script
# ==============================================================================

set -e

echo "[GW BUILD] Starting Gateway Network Protocol build on Linux..."

if command -v cmake >/dev/null 2>&1; then
    echo "[GW BUILD] CMake detected. Building using CMake..."
    mkdir -p build
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j"$(nproc)"
    echo "[GW BUILD] Running tests via CTest..."
    ctest --test-dir build --output-on-failure
    echo "[GW BUILD] Build and tests completed successfully!"
    exit 0
fi

if command -v gcc >/dev/null 2>&1; then
    echo "[GW BUILD] GCC detected. Compiling targets directly..."

    CFLAGS="-std=c11 -Wall -Wextra -pedantic -Iinclude"
    LIBS="-lpthread"

    COMMON_SRCS="src/common/platform.c src/common/logger.c src/common/crc8.c src/common/crypto.c src/common/transform.c src/common/packet_parser.c src/common/stream_framer.c"
    SERVER_SRCS="src/server/validator.c src/server/response_builder.c src/server/tcp_server.c src/server/conn_handler.c src/server/server_metrics.c"
    CLIENT_SRCS="src/client/request_builder.c src/client/response_parser.c src/client/tcp_client.c"
    TEST_SRCS="tests/unity/unity.c tests/test_platform.c tests/test_parser.c tests/test_validator.c tests/test_crypto.c tests/test_stream_framer.c tests/test_integration.c tests/test_concurrent.c tests/test_fuzz.c tests/test_runner.c"

    echo "[GW BUILD] Compiling gw_server..."
    gcc $CFLAGS $COMMON_SRCS $SERVER_SRCS src/server/server_main.c $LIBS -o gw_server

    echo "[GW BUILD] Compiling gw_client..."
    gcc $CFLAGS $COMMON_SRCS $CLIENT_SRCS src/client/client_main.c $LIBS -o gw_client

    echo "[GW BUILD] Compiling gw_tests..."
    gcc $CFLAGS -Itests $COMMON_SRCS $SERVER_SRCS $CLIENT_SRCS $TEST_SRCS $LIBS -o gw_tests

    echo "[GW BUILD] Running test suite (gw_tests)..."
    ./gw_tests

    echo "[GW BUILD] All targets built and all tests passed successfully!"
    exit 0
fi

echo "[GW BUILD] Error: Neither CMake nor GCC found on PATH." >&2
exit 1
