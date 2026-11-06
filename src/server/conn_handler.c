/**
 * @file conn_handler.c
 * @brief Per-connection worker thread: full GW protocol pipeline implementation.
 *
 * Each worker thread:
 *   1. Reads a complete frame using two-phase stream framing
 *   2. Parses the 8-byte header
 *   3. Validates (length -> SA -> TA -> CRC) per priority
 *   4. On validation failure: sends Negative Response (10 bytes)
 *   5. On validation success:
 *      - Decrypts payload (XOR 0xAA)
 *      - Swaps nibbles on each byte
 *      - Encrypts result (XOR 0x55)
 *      - Computes CRC-8 over encrypted response payload
 *      - Sends Positive Response
 *   6. Repeats until client disconnects or irrecoverable error
 */
#include "server/conn_handler.h"
#include "server/validator.h"
#include "server/response_builder.h"
#include "server/server_metrics.h"
#include "common/stream_framer.h"
#include "common/packet_parser.h"
#include "common/crypto.h"
#include "common/transform.h"
#include "common/crc8.h"
#include "common/logger.h"
#include "common/gw_types.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Working buffers per connection (stack-allocated - no heap inside worker) */
#define CONN_RECV_BUF_SIZE  (GW_MAX_FRAME_SIZE + 16)
#define CONN_SEND_BUF_SIZE  (GW_MAX_FRAME_SIZE + 16)

#define CONN_TAG "CONN"

/* =========================================================================
 * Internal Helper: Process One Request Packet
 * ========================================================================= */

/**
 * @brief Process one received frame: validate -> transform -> respond.
 *
 * @param ctx   Connection context
 * @param buf   Received raw frame bytes (mutable)
 * @param total Total frame bytes received
 * @return 0 to continue processing, -1 to close connection
 */
static int process_frame(const gw_conn_ctx_t *ctx,
                          uint8_t *buf, size_t total) {
    gw_metrics_inc_requests();

    /* --- Log received hex dump (DEBUG level) --- */
    gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "RX", buf, total);

    /* --- Parse packet header --- */
    gw_packet_t pkt;
    if (gw_parse_packet(buf, total, &pkt) != 0) {
        /* Cannot parse even the header - length mismatch */
        GW_LOG_W(CONN_TAG, "[#%d] Header parse failed (total=%zu). Sending len-mismatch.",
                 ctx->conn_id, total);

        uint8_t send_buf[CONN_SEND_BUF_SIZE];
        size_t out_len = 0;
        gw_build_negative_response(NULL, GW_FAIL_LENGTH_MISMATCH,
                                   send_buf, sizeof(send_buf), &out_len);
        gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "TX-NEG", send_buf, out_len);
        gw_send_exact(ctx->client_sock, send_buf, out_len);
        gw_metrics_inc_negative(GW_FAIL_LENGTH_MISMATCH);
        return 0; /* Keep connection alive; next frame may be valid */
    }

    /* --- Run 4-stage validation pipeline --- */
    gw_failure_code_t fail = gw_validate_packet(buf, total, &pkt);

    if (fail != GW_FAIL_NONE) {
        /* Negative Response */
        GW_LOG_I(CONN_TAG, "[#%d] Validation FAILED code=0x%02X SA=0x%04X TA=0x%04X len=%u",
                 ctx->conn_id, (unsigned)fail,
                 pkt.source_addr, pkt.target_addr, pkt.length);

        uint8_t send_buf[CONN_SEND_BUF_SIZE];
        size_t out_len = 0;
        gw_build_negative_response(&pkt, fail,
                                   send_buf, sizeof(send_buf), &out_len);
        gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "TX-NEG", send_buf, out_len);
        gw_send_exact(ctx->client_sock, send_buf, out_len);
        gw_metrics_inc_negative(fail);
        return 0;
    }

    /* --- Positive path: decrypt -> transform -> encrypt --- */
    GW_LOG_I(CONN_TAG, "[#%d] Validation OK SA=0x%04X TA=0x%04X payload_len=%u",
             ctx->conn_id, pkt.source_addr, pkt.target_addr, pkt.payload_len);

    /* Work on a local mutable copy of the payload */
    uint8_t work_buf[GW_MAX_PAYLOAD_SIZE];
    if (pkt.payload_len > sizeof(work_buf)) {
        GW_LOG_E(CONN_TAG, "[#%d] payload_len %u > work buffer - closing.",
                 ctx->conn_id, pkt.payload_len);
        return -1;
    }
    memcpy(work_buf, pkt.payload, pkt.payload_len);

    /* Step 1: Decrypt incoming payload (XOR 0xAA) */
    gw_decrypt_request_payload(work_buf, pkt.payload_len);
    if (ctx->verbose) {
        gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "DECRYPTED", work_buf, pkt.payload_len);
    }

    /* Step 2: Nibble swap each byte */
    gw_swap_nibbles(work_buf, pkt.payload_len);
    if (ctx->verbose) {
        gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "SWAPPED", work_buf, pkt.payload_len);
    }

    /* Step 3: Encrypt response payload (XOR 0x55) */
    gw_encrypt_response_payload(work_buf, pkt.payload_len);
    if (ctx->verbose) {
        gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "RESP-ENC", work_buf, pkt.payload_len);
    }

    /* Step 4: Build and send Positive Response */
    uint8_t send_buf[CONN_SEND_BUF_SIZE];
    size_t out_len = 0;
    if (gw_build_positive_response(&pkt, work_buf, (uint32_t)pkt.payload_len,
                                   send_buf, sizeof(send_buf), &out_len) != 0) {
        GW_LOG_E(CONN_TAG, "[#%d] Failed to build positive response.", ctx->conn_id);
        return -1;
    }

    gw_log_hex_dump(GW_LOG_DEBUG, CONN_TAG, "TX-POS", send_buf, out_len);

    if (gw_send_exact(ctx->client_sock, send_buf, out_len) != 0) {
        GW_LOG_W(CONN_TAG, "[#%d] Send failed (client disconnected?).", ctx->conn_id);
        return -1;
    }

    gw_metrics_inc_positive();
    GW_LOG_I(CONN_TAG, "[#%d] Positive response sent (%zu bytes).",
             ctx->conn_id, out_len);
    return 0;
}

/* =========================================================================
 * Worker Thread Entry Point
 * ========================================================================= */

void *gw_conn_handler_run(void *arg) {
    gw_conn_ctx_t *ctx = (gw_conn_ctx_t *)arg;
    if (!ctx) return NULL;

    GW_LOG_I(CONN_TAG, "[#%d] Worker thread started.", ctx->conn_id);

    uint8_t recv_buf[CONN_RECV_BUF_SIZE];
    size_t total_read = 0;
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 3;

    while (1) {
        total_read = 0;
        int rc = gw_read_frame(ctx->client_sock, recv_buf, sizeof(recv_buf), &total_read);

        if (rc == 1) {
            /* Clean client disconnect */
            GW_LOG_I(CONN_TAG, "[#%d] Client disconnected gracefully.", ctx->conn_id);
            break;
        }
        if (rc == -2) {
            /* Frame too large for buffer - send length error, continue */
            GW_LOG_W(CONN_TAG, "[#%d] Oversized frame received. Sending length mismatch.",
                     ctx->conn_id);
            gw_metrics_inc_requests();
            uint8_t send_buf[CONN_SEND_BUF_SIZE];
            size_t out_len = 0;
            gw_build_negative_response(NULL, GW_FAIL_LENGTH_MISMATCH,
                                       send_buf, sizeof(send_buf), &out_len);
            gw_send_exact(ctx->client_sock, send_buf, out_len);
            gw_metrics_inc_negative(GW_FAIL_LENGTH_MISMATCH);
            consecutive_errors++;
        } else if (rc < 0) {
            /* Socket read error */
            GW_LOG_W(CONN_TAG, "[#%d] gw_read_frame() error (rc=%d). Closing.",
                     ctx->conn_id, rc);
            break;
        } else {
            /* Successfully received a frame */
            consecutive_errors = 0;
            if (process_frame(ctx, recv_buf, total_read) != 0) {
                GW_LOG_W(CONN_TAG, "[#%d] process_frame() error. Closing.", ctx->conn_id);
                break;
            }
        }

        if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
            GW_LOG_E(CONN_TAG, "[#%d] Too many consecutive errors (%d). Closing.",
                     ctx->conn_id, consecutive_errors);
            break;
        }
    }

    GW_LOG_I(CONN_TAG, "[#%d] Worker thread exiting.", ctx->conn_id);
    gw_socket_close(ctx->client_sock);
    free(ctx);
    return NULL;
}
