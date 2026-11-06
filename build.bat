@echo off
rem ============================================================================
rem Gateway (GW) Network Packet Communication System - Windows Build Script
rem ============================================================================

setlocal
echo [GW BUILD] Starting Gateway Network Protocol build on Windows...

rem Check if CMake is available
where cmake >nul 2>nul
if %ERRORLEVEL% equ 0 goto USE_CMAKE

rem Check if GCC is available
where gcc >nul 2>nul
if %ERRORLEVEL% equ 0 goto USE_GCC

echo [GW BUILD] Error: Neither CMake nor GCC found on PATH.
exit /b 1

:USE_CMAKE
echo [GW BUILD] CMake detected. Building using CMake...
if not exist build mkdir build
cmake -B build -S .
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] CMake configure failed.
    exit /b 1
)
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] CMake build failed.
    exit /b 1
)
echo [GW BUILD] Running tests via CTest...
ctest --test-dir build -C Release --output-on-failure
echo [GW BUILD] Build and tests completed successfully!
exit /b 0

:USE_GCC
echo [GW BUILD] MinGW GCC detected. Compiling targets directly...

set CFLAGS=-std=c11 -Wall -Wextra -pedantic -Iinclude
set LIBS=-lws2_32

set COMMON_SRCS=src/common/platform.c src/common/logger.c src/common/crc8.c src/common/crypto.c src/common/transform.c src/common/packet_parser.c src/common/stream_framer.c
set SERVER_SRCS=src/server/validator.c src/server/response_builder.c src/server/tcp_server.c src/server/conn_handler.c src/server/server_metrics.c
set CLIENT_SRCS=src/client/request_builder.c src/client/response_parser.c src/client/tcp_client.c
set TEST_SRCS=tests/unity/unity.c tests/test_platform.c tests/test_parser.c tests/test_validator.c tests/test_crypto.c tests/test_stream_framer.c tests/test_integration.c tests/test_concurrent.c tests/test_fuzz.c tests/test_runner.c

echo [GW BUILD] Compiling gw_server.exe...
gcc %CFLAGS% %COMMON_SRCS% %SERVER_SRCS% src/server/server_main.c %LIBS% -o gw_server.exe
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] Failed to build gw_server.exe
    exit /b 1
)

echo [GW BUILD] Compiling gw_client.exe...
gcc %CFLAGS% %COMMON_SRCS% %CLIENT_SRCS% src/client/client_main.c %LIBS% -o gw_client.exe
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] Failed to build gw_client.exe
    exit /b 1
)

echo [GW BUILD] Compiling gw_tests.exe...
gcc %CFLAGS% -Itests %COMMON_SRCS% %SERVER_SRCS% %CLIENT_SRCS% %TEST_SRCS% %LIBS% -o gw_tests.exe
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] Failed to build gw_tests.exe
    exit /b 1
)

echo [GW BUILD] Running test suite...
.\gw_tests.exe
if %ERRORLEVEL% neq 0 (
    echo [GW BUILD] Unit and Integration tests failed!
    exit /b 1
)

echo [GW BUILD] All targets built and all tests passed successfully!
exit /b 0
