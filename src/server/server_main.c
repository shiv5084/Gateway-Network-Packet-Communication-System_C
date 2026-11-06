/**
 * @file server_main.c
 * @brief Entry point for the Gateway (GW) TCP Server executable (Phase 5).
 */
#include "server/tcp_server.h"
#include "server/server_metrics.h"
#include "common/platform.h"
#include "common/logger.h"
#include "common/gw_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GW_SERVER_VERSION "1.0.0"

#if defined(_WIN32) || defined(_WIN64)
    #define GW_OS_NAME "Windows (Winsock2)"
#elif defined(__linux__)
    #define GW_OS_NAME "Linux (POSIX)"
#else
    #define GW_OS_NAME "Generic Platform"
#endif

static void print_usage(const char *prog) {
    printf("Gateway Network Server v%s\n", GW_SERVER_VERSION);
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --port <port>       Port to listen on (default: %u)\n", GW_DEFAULT_PORT);
    printf("  --ip <ip>           IP address to bind to (default: 0.0.0.0)\n");
    printf("  --log-file <file>   Optional path to write log output\n");
    printf("  --verbose           Enable debug logging and byte tracing\n");
    printf("  --help              Display this help message\n");
}

static void print_banner(uint16_t port, const char *ip, bool verbose, const char *log_file) {
    printf("============================================================\n");
    printf(" Gateway (GW) Network Packet Server v%s\n", GW_SERVER_VERSION);
    printf(" Platform:      %s\n", GW_OS_NAME);
    printf(" Listening On:  %s:%u\n", ip, port);
    printf(" Log Level:     %s\n", verbose ? "DEBUG (Live byte tracing)" : "INFO");
    if (log_file) {
        printf(" Log File:      %s\n", log_file);
    }
    printf(" Protocol:      TCP Port %u | Big-Endian Header\n", port);
    printf(" Press Ctrl+C for clean shutdown with telemetry dump.\n");
    printf("============================================================\n\n");
}

int main(int argc, char *argv[]) {
    uint16_t port = GW_DEFAULT_PORT;
    const char *ip = "0.0.0.0";
    const char *log_file = NULL;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
            ip = argv[++i];
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            log_file = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (gw_platform_init() != 0) {
        fprintf(stderr, "Failed to initialize platform network layer.\n");
        return 1;
    }

    gw_log_init(verbose ? GW_LOG_DEBUG : GW_LOG_INFO, log_file);
    print_banner(port, ip, verbose, log_file);

    gw_server_t server;
    gw_server_init(&server, port, ip, verbose);

    if (gw_server_start(&server) != 0) {
        fprintf(stderr, "Failed to start server on %s:%u\n", ip, port);
        gw_log_close();
        gw_platform_cleanup();
        return 1;
    }

    gw_server_install_signal_handlers(&server);

    /* Run the main accept loop (blocks until Ctrl+C / stop) */
    gw_server_run(&server);

    /* Clean shutdown sequence */
    gw_server_stop(&server);

    /* Dump telemetry metrics per Phase 5 Exit Criteria */
    gw_metrics_dump();

    gw_log_close();
    gw_platform_cleanup();
    printf("Gateway Server shut down cleanly.\n");

    return 0;
}
