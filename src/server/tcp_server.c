/**
 * @file tcp_server.c
 * @brief Gateway TCP Server implementation.
 *
 * Implements socket lifecycle (create, bind, listen, accept), per-connection
 * thread dispatch, and cross-platform graceful shutdown via signal handlers.
 */
#include "server/tcp_server.h"
#include "server/conn_handler.h"
#include "server/server_metrics.h"
#include "common/logger.h"
#include "common/platform.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Signal Handling - Platform Specific
 * ========================================================================= */

/** Global pointer to the server for signal handler callback */
static gw_server_t *g_signal_srv = NULL;

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static BOOL WINAPI gw_win_ctrl_handler(DWORD ctrl_type) {
    if ((ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT ||
         ctrl_type == CTRL_CLOSE_EVENT) && g_signal_srv) {
        GW_LOG_I("SERVER", "Shutdown signal received (Ctrl+C / Close).");
        gw_server_stop(g_signal_srv);
        return TRUE;
    }
    return FALSE;
}
#else
#include <signal.h>
static void gw_posix_signal_handler(int signum) {
    (void)signum;
    if (g_signal_srv) {
        GW_LOG_I("SERVER", "Shutdown signal received (SIGINT/SIGTERM).");
        gw_server_stop(g_signal_srv);
    }
}
#endif

/* =========================================================================
 * Connection ID Counter
 * ========================================================================= */
static int s_conn_id_counter = 0;

/* =========================================================================
 * Server API Implementation
 * ========================================================================= */

void gw_server_init(gw_server_t *srv, uint16_t port, const char *ip, bool verbose) {
    if (!srv) return;
    memset(srv, 0, sizeof(*srv));
    srv->listen_sock = GW_INVALID_SOCKET;
    srv->port        = (port != 0) ? port : GW_DEFAULT_PORT;
    srv->bind_ip     = (ip   != NULL) ? ip : "0.0.0.0";
    srv->running     = false;
    srv->verbose     = verbose;
}

int gw_server_start(gw_server_t *srv) {
    if (!srv) return -1;

    gw_metrics_init();

    /* Create TCP socket */
    srv->listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!gw_socket_is_valid(srv->listen_sock)) {
        char errbuf[128];
        int err = gw_socket_get_last_error();
        GW_LOG_E("SERVER", "socket() failed: %s",
                 gw_socket_strerror(err, errbuf, sizeof(errbuf)));
        return -1;
    }

    /* SO_REUSEADDR: allow rapid restart without TIME_WAIT delays */
    if (gw_socket_set_reuseaddr(srv->listen_sock, true) != 0) {
        GW_LOG_W("SERVER", "setsockopt(SO_REUSEADDR) failed, continuing...");
    }

    /* Bind to configured IP:port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(srv->port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (srv->bind_ip && strcmp(srv->bind_ip, "0.0.0.0") != 0) {
        addr.sin_addr.s_addr = inet_addr(srv->bind_ip);
    }

    if (bind(srv->listen_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        char errbuf[128];
        int err = gw_socket_get_last_error();
        GW_LOG_E("SERVER", "bind() failed on %s:%u: %s",
                 srv->bind_ip, srv->port,
                 gw_socket_strerror(err, errbuf, sizeof(errbuf)));
        gw_socket_close(srv->listen_sock);
        srv->listen_sock = GW_INVALID_SOCKET;
        return -1;
    }

    /* Listen with configured backlog */
    if (listen(srv->listen_sock, GW_SERVER_LISTEN_BACKLOG) != 0) {
        char errbuf[128];
        int err = gw_socket_get_last_error();
        GW_LOG_E("SERVER", "listen() failed: %s",
                 gw_socket_strerror(err, errbuf, sizeof(errbuf)));
        gw_socket_close(srv->listen_sock);
        srv->listen_sock = GW_INVALID_SOCKET;
        return -1;
    }

    srv->running = true;
    GW_LOG_I("SERVER", "Gateway Server listening on %s:%u (backlog=%d)",
             srv->bind_ip, srv->port, GW_SERVER_LISTEN_BACKLOG);
    return 0;
}

void gw_server_run(gw_server_t *srv) {
    if (!srv || !gw_socket_is_valid(srv->listen_sock)) return;

    GW_LOG_I("SERVER", "Accept loop started. Waiting for connections...");

    while (srv->running) {
        struct sockaddr_in client_addr;
#if defined(_WIN32) || defined(_WIN64)
        int addr_len = (int)sizeof(client_addr);
#else
        socklen_t addr_len = (socklen_t)sizeof(client_addr);
#endif
        gw_socket_t client_sock = accept(srv->listen_sock,
                                         (struct sockaddr *)&client_addr,
                                         &addr_len);

        if (!gw_socket_is_valid(client_sock)) {
            /* Check if server was stopped (accept() interrupted by close) */
            if (!srv->running) {
                GW_LOG_I("SERVER", "Accept loop terminated (server stopping).");
                break;
            }
            char errbuf[128];
            int err = gw_socket_get_last_error();
            GW_LOG_W("SERVER", "accept() returned invalid socket: %s",
                     gw_socket_strerror(err, errbuf, sizeof(errbuf)));
            continue;
        }

        /* Set per-connection receive/send timeout */
        gw_socket_set_timeouts(client_sock, GW_SERVER_CONN_TIMEOUT_MS);

        /* Build connection context and spawn worker thread */
        gw_conn_ctx_t *ctx = (gw_conn_ctx_t *)malloc(sizeof(gw_conn_ctx_t));
        if (!ctx) {
            GW_LOG_E("SERVER", "malloc(gw_conn_ctx_t) failed - dropping connection.");
            gw_socket_close(client_sock);
            continue;
        }
        ctx->client_sock = client_sock;
        ctx->verbose     = srv->verbose;
        ctx->conn_id     = ++s_conn_id_counter;

        char ip_str[INET_ADDRSTRLEN];
        const char *ip_result = inet_ntoa(client_addr.sin_addr);
        if (ip_result) {
            snprintf(ip_str, sizeof(ip_str), "%s", ip_result);
        } else {
            snprintf(ip_str, sizeof(ip_str), "?.?.?.?");
        }
        GW_LOG_I("SERVER", "[Conn #%d] Accepted from %s:%u",
                 ctx->conn_id, ip_str, ntohs(client_addr.sin_port));
        gw_metrics_inc_connections();

        gw_thread_t thread;
        if (gw_thread_create(&thread, gw_conn_handler_run, ctx) != 0) {
            GW_LOG_E("SERVER", "[Conn #%d] Thread spawn failed - dropping connection.",
                     ctx->conn_id);
            gw_socket_close(client_sock);
            free(ctx);
            continue;
        }

        /* Detach: let the thread clean up its own resources */
#if defined(_WIN32) || defined(_WIN64)
        CloseHandle(thread); /* Detach on Windows by closing handle */
#else
        pthread_detach(thread);
#endif
    }

    GW_LOG_I("SERVER", "Accept loop exited.");
}

void gw_server_stop(gw_server_t *srv) {
    if (!srv) return;
    srv->running = false;
    /* Force accept() to unblock by closing the listening socket */
    if (gw_socket_is_valid(srv->listen_sock)) {
        gw_socket_close(srv->listen_sock);
        srv->listen_sock = GW_INVALID_SOCKET;
    }
}

void gw_server_install_signal_handlers(gw_server_t *srv) {
    g_signal_srv = srv;
#if defined(_WIN32) || defined(_WIN64)
    SetConsoleCtrlHandler(gw_win_ctrl_handler, TRUE);
    GW_LOG_D("SERVER", "Windows console control handler installed.");
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gw_posix_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    GW_LOG_D("SERVER", "POSIX SIGINT/SIGTERM handlers installed.");
#endif
}
