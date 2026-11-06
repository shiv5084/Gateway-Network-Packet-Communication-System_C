/**
 * @file tcp_server.h
 * @brief Gateway TCP Server - Socket lifecycle, accept loop, and signal handling.
 *
 * Manages the server listening socket on port 13400, accepts incoming client
 * connections, spawns per-connection worker threads, and handles graceful shutdown.
 *
 * Cross-platform: supports both POSIX (Linux) and Winsock2 (Windows).
 */
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "common/platform.h"
#include "common/gw_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Server Configuration
 * ========================================================================= */

/** Maximum number of pending connections in the listen backlog */
#define GW_SERVER_LISTEN_BACKLOG    32

/** Default connection receive/send timeout (ms) */
#define GW_SERVER_CONN_TIMEOUT_MS   30000

/* =========================================================================
 * Server State Structure
 * ========================================================================= */

/**
 * @brief Encapsulates all server runtime state.
 */
typedef struct {
    gw_socket_t   listen_sock;      /**< Main listening socket */
    uint16_t      port;             /**< Listening port (default: 13400) */
    const char   *bind_ip;          /**< Bind address (default: "0.0.0.0") */
    bool          running;          /**< Server accept-loop active flag (volatile) */
    bool          verbose;          /**< Enable verbose debug logging */
} gw_server_t;

/* =========================================================================
 * Server Lifecycle API
 * ========================================================================= */

/**
 * @brief Initializes the server state structure with default values.
 *
 * @param srv Pointer to server state to initialize
 * @param port  TCP port to listen on (pass 0 for GW_DEFAULT_PORT)
 * @param ip    Bind IP address string (pass NULL for "0.0.0.0")
 * @param verbose Enable verbose logging
 */
void gw_server_init(gw_server_t *srv, uint16_t port, const char *ip, bool verbose);

/**
 * @brief Creates, binds, and begins listening on the TCP server socket.
 *
 * Sets SO_REUSEADDR. Binds to configured IP:port. Listens with
 * GW_SERVER_LISTEN_BACKLOG pending connections.
 *
 * @param srv Initialized server state
 * @return 0 on success, -1 on socket creation/bind/listen failure
 */
int gw_server_start(gw_server_t *srv);

/**
 * @brief Enters the accept-loop, spawning a worker thread per connection.
 *
 * Blocks until gw_server_stop() is called (sets srv->running = false) or
 * a fatal socket error occurs. Each accepted client is dispatched to
 * gw_conn_handler_run() in a dedicated thread.
 *
 * @param srv Running server state
 */
void gw_server_run(gw_server_t *srv);

/**
 * @brief Signals the accept-loop to stop. Thread-safe.
 *
 * Sets srv->running = false and forces the blocking accept() to unblock
 * by closing the listening socket.
 *
 * @param srv Running server state
 */
void gw_server_stop(gw_server_t *srv);

/**
 * @brief Installs platform signal handlers for SIGINT/SIGTERM (Linux) and
 *        SetConsoleCtrlHandler (Windows) to trigger graceful shutdown.
 *
 * @param srv Server instance to stop when signal is received
 */
void gw_server_install_signal_handlers(gw_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* TCP_SERVER_H */
