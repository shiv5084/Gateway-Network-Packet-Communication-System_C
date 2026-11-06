/**
 * @file tcp_client.h
 * @brief Gateway TCP Diagnostic Client - connection management and request/response API.
 *
 * Provides a clean API for connecting to the GW Server, sending protocol
 * request frames, and receiving/decoding response frames.
 *
 * Cross-platform: supports both POSIX (Linux) and Winsock2 (Windows).
 *
 * NOTE: Response data is returned as gw_client_response_t defined in
 *       client/response_parser.h. Include that header for full response details.
 */
#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "common/platform.h"
#include "common/gw_types.h"
#include "client/response_parser.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Client Constants
 * ========================================================================= */

/** Default connection receive/send timeout in milliseconds */
#define GW_CLIENT_RECV_TIMEOUT_MS       5000

/* =========================================================================
 * Client State Structure
 * ========================================================================= */

/**
 * @brief Encapsulates TCP client connection state.
 */
typedef struct {
    gw_socket_t  sock;          /**< Connected TCP socket */
    bool         connected;     /**< Connection active flag */
    char         server_ip[64]; /**< Server IP address string */
    uint16_t     server_port;   /**< Server port */
    bool         verbose;       /**< Enable verbose debug logging */
} gw_client_t;

/* =========================================================================
 * Client Lifecycle API
 * ========================================================================= */

/**
 * @brief Initializes a GW client state structure.
 *
 * @param client Pointer to client state to initialize
 * @param verbose Enable verbose logging
 */
void gw_client_init(gw_client_t *client, bool verbose);

/**
 * @brief Establishes a TCP connection to the GW server.
 *
 * Resolves the server address, creates a socket, and connects.
 * Sets configurable receive/send timeouts.
 *
 * @param client Initialized client state
 * @param ip     Server IP address (e.g. "127.0.0.1")
 * @param port   Server port (e.g. 13400)
 * @return 0 on success, -1 on connection failure
 */
int gw_client_connect(gw_client_t *client, const char *ip, uint16_t port);

/**
 * @brief Sends a raw pre-built request frame and reads the decoded response.
 *
 * Sends the request frame over the connected socket, then uses two-phase
 * stream framing to receive the complete response, and decodes it into
 * a gw_client_response_t struct (see client/response_parser.h).
 *
 * @param client       Connected client state
 * @param req_bytes    Pre-built request frame bytes
 * @param req_len      Number of request frame bytes
 * @param resp_out     Output: decoded response structure
 * @return 0 on success (response received and parsed),
 *        -1 on send/receive error or parse failure
 */
int gw_client_send_request(gw_client_t *client,
                           const uint8_t *req_bytes, size_t req_len,
                           gw_client_response_t *resp_out);

/**
 * @brief Closes the TCP connection and resets client state.
 *
 * @param client Connected client state
 */
void gw_client_disconnect(gw_client_t *client);

/**
 * @brief Prints a formatted summary of a received response to stdout.
 *
 * @param resp Response to display
 */
void gw_client_print_response(const gw_client_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* TCP_CLIENT_H */
