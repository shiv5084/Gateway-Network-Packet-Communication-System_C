cmake_minimum_required(VERSION 3.15)
project(GW_Network_Packet_Communication C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories
include_directories(
    include
    tests
)

# Compiler warnings and options
if(MSVC)
    add_compile_options(/W4 /WX /wd4996)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread)
endif()

# ==============================================================================
# Common Core Library (libgw_common)
# ==============================================================================
set(COMMON_SOURCES
    src/common/platform.c
    src/common/logger.c
    src/common/crc8.c
    src/common/crypto.c
    src/common/transform.c
    src/common/packet_parser.c
    src/common/stream_framer.c
)

add_library(gw_common STATIC ${COMMON_SOURCES})
target_link_libraries(gw_common PUBLIC ${PLATFORM_LIBS})

# ==============================================================================
# Server Modules & Executable
# ==============================================================================
set(SERVER_SOURCES
    src/server/validator.c
    src/server/response_builder.c
    src/server/tcp_server.c
    src/server/conn_handler.c
    src/server/server_metrics.c
)

add_executable(gw_server src/server/server_main.c ${SERVER_SOURCES})
target_link_libraries(gw_server PRIVATE gw_common ${PLATFORM_LIBS})

# ==============================================================================
# Client Modules & Executable
# ==============================================================================
set(CLIENT_SOURCES
    src/client/request_builder.c
    src/client/response_parser.c
    src/client/tcp_client.c
)

add_executable(gw_client src/client/client_main.c ${CLIENT_SOURCES})
target_link_libraries(gw_client PRIVATE gw_common ${PLATFORM_LIBS})

# ==============================================================================
# Unit & Integration Test Runner
# ==============================================================================
enable_testing()

set(TEST_SOURCES
    tests/unity/unity.c
    tests/test_platform.c
    tests/test_parser.c
    tests/test_validator.c
    tests/test_crypto.c
    tests/test_stream_framer.c
    tests/test_integration.c
    tests/test_concurrent.c
    tests/test_fuzz.c
    tests/test_runner.c
    ${SERVER_SOURCES}
    ${CLIENT_SOURCES}
)

add_executable(gw_tests ${TEST_SOURCES})
target_link_libraries(gw_tests PRIVATE gw_common ${PLATFORM_LIBS})

add_test(NAME All_Tests COMMAND gw_tests)
