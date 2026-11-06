/**
 * @file conn_handler.h
 * @brief Per-connection worker thread: reads, validates, transforms, and responds.
 *
 * Each accepted client connection is handled by a dedicated worker thread.
 * The handler executes the full GW protocol pipeline:
 *   1. Read full frame via gw_read_frame()
 *   2. Parse header via gw_parse_packet()
 *   3. Validate via gw_validate_packet()
 *   4. On failure -> send Negative Response (10 bytes)
 *   5. On success -> decrypt (XOR 0xAA) -> swap nibbles -> encrypt (XOR 0x55) ->
 *                   compute CRC -> send Positive Response
 *   6. Repeat until client disconnects or irrecoverable error
 */
#ifndef CONN_HANDLER_H
#define CONN_HANDLER_H

#include "common/platform.h"
#include "common/gw_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Connection Context
 * ========================================================================= */

/**
 * @brief Context passed to each worker thread.
 *        Heap-allocated by the accept loop; freed by the worker on exit.
 */
typedef struct {
    gw_socket_t  client_sock;   /**< Connected client socket */
    bool         verbose;        /**< Verbose debug logging */
    int          conn_id;        /**< Monotonically-increasing connection ID */
} gw_conn_ctx_t;

/* =========================================================================
 * Worker Thread Entry Point
 * ========================================================================= */

/**
 * @brief Worker thread entry point for handling a single client connection.
 *
 * This function runs the entire per-connection GW protocol processing loop.
 * It owns the gw_conn_ctx_t allocation and frees it before returning.
 *
 * Signature matches gw_thread_fn_t: void* fn(void*).
 *
 * @param arg Pointer to heap-allocated gw_conn_ctx_t
 * @return NULL always
 */
void *gw_conn_handler_run(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* CONN_HANDLER_H */
