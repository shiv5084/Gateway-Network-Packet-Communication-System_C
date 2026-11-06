/**
 * @file tcp_client.c
 * @brief Gateway TCP Diagnostic Client implementation.
 *
 * Implements connection management, request sending, and response decoding.
 * Uses the common stream_framer for two-phase receive and response_parser
 * for decoding incoming response frames.
 */
#include "client/tcp_client.h"
#include "client/request_builder.h"
#include "client/response_parser.h"
#include "common/stream_framer.h"
#include "common/logger.h"
#include "common/platform.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CLIENT_TAG "CLIENT"

/* =========================================================================
 * Client API Implementation
 * ========================================================================= */

void gw_client_init(gw_client_t *client, bool verbose) {
    if (!client) return;
    memset(client, 0, sizeof(*client));
    client->sock      = GW_INVALID_SOCKET;
    client->connected = false;
    client->verbose   = verbose;
}

int gw_client_connect(gw_client_t *client, const char *ip, uint16_t port) {
    if (!client || !ip || port == 0) return -1;

    /* Store connection parameters */
    snprintf(client->server_ip, sizeof(client->server_ip), "%s", ip);
    client->server_port = port;

    /* Create socket */
    client->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!gw_socket_is_valid(client->sock)) {
        char errbuf[128];
        int err = gw_socket_get_last_error();
        GW_LOG_E(CLIENT_TAG, "socket() failed: %s",
                 gw_socket_strerror(err, errbuf, sizeof(errbuf)));
        return -1;
    }

    /* Set receive/send timeouts */
    gw_socket_set_timeouts(client->sock, GW_CLIENT_RECV_TIMEOUT_MS);

    /* Resolve and connect */
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(port);
    srv_addr.sin_addr.s_addr = inet_addr(ip);

    if (srv_addr.sin_addr.s_addr == INADDR_NONE) {
        GW_LOG_E(CLIENT_TAG, "Invalid IP address: %s", ip);
        gw_socket_close(client->sock);
        client->sock = GW_INVALID_SOCKET;
        return -1;
    }

    if (connect(client->sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) != 0) {
        char errbuf[128];
        int err = gw_socket_get_last_error();
        GW_LOG_E(CLIENT_TAG, "connect() to %s:%u failed: %s",
                 ip, port, gw_socket_strerror(err, errbuf, sizeof(errbuf)));
        gw_socket_close(client->sock);
        client->sock = GW_INVALID_SOCKET;
        return -1;
    }

    client->connected = true;
    GW_LOG_I(CLIENT_TAG, "Connected to GW Server at %s:%u", ip, port);
    return 0;
}

int gw_client_send_request(gw_client_t *client,
                           const uint8_t *req_bytes, size_t req_len,
                           gw_client_response_t *resp_out) {
    if (!client || !client->connected || !gw_socket_is_valid(client->sock)) {
        GW_LOG_E(CLIENT_TAG, "Not connected.");
        return -1;
    }
    if (!req_bytes || req_len == 0 || !resp_out) {
        return -1;
    }

    /* Hex dump outgoing request */
    gw_log_hex_dump(GW_LOG_DEBUG, CLIENT_TAG, "TX-REQ", req_bytes, req_len);

    /* Send the complete request frame */
    if (gw_send_exact(client->sock, req_bytes, req_len) != 0) {
        GW_LOG_E(CLIENT_TAG, "gw_send_exact() failed - connection lost?");
        client->connected = false;
        return -1;
    }

    /* Receive response frame using two-phase framing */
    uint8_t resp_buf[GW_MAX_FRAME_SIZE + 16];
    size_t total_read = 0;
    int rc = gw_read_frame(client->sock, resp_buf, sizeof(resp_buf), &total_read);

    if (rc != 0) {
        if (rc == 1) {
            GW_LOG_W(CLIENT_TAG, "Server closed connection unexpectedly.");
        } else {
            GW_LOG_E(CLIENT_TAG, "gw_read_frame() error (rc=%d).", rc);
        }
        client->connected = false;
        return -1;
    }

    gw_log_hex_dump(GW_LOG_DEBUG, CLIENT_TAG, "RX-RESP", resp_buf, total_read);

    /* Decode the response using response_parser */
    if (gw_client_parse_response(resp_buf, total_read, resp_out) != 0) {
        GW_LOG_E(CLIENT_TAG, "Response parse failed.");
        return -1;
    }

    if (resp_out->type == GW_RESP_NEGATIVE) {
        GW_LOG_I(CLIENT_TAG, "Received NEGATIVE response. FailCode=0x%02X",
                 (unsigned)resp_out->failure_code);
    } else {
        GW_LOG_I(CLIENT_TAG, "Received POSITIVE response. payload_len=%u",
                 resp_out->payload_len);
    }

    return 0;
}

void gw_client_disconnect(gw_client_t *client) {
    if (!client) return;
    if (gw_socket_is_valid(client->sock)) {
        gw_socket_close(client->sock);
        client->sock = GW_INVALID_SOCKET;
    }
    client->connected = false;
    GW_LOG_I(CLIENT_TAG, "Disconnected from GW Server.");
}

void gw_client_print_response(const gw_client_response_t *resp) {
    if (!resp) return;

    printf("\n--- GW Server Response ---\n");
    printf("  SA:   0x%04X\n", resp->source_addr);
    printf("  TA:   0x%04X\n", resp->target_addr);
    printf("  CRC:  0x%02X (%s)\n", resp->crc, resp->crc_valid ? "VALID" : "INVALID");

    if (resp->type == GW_RESP_NEGATIVE) {
        printf("  Type: NEGATIVE RESPONSE\n");
        printf("  Fail: 0x%02X", (unsigned)resp->failure_code);

        switch (resp->failure_code) {
            case GW_FAIL_SRC_ADDR_INVALID:
                printf(" (Invalid Source Address)\n");
                break;
            case GW_FAIL_TGT_ADDR_INVALID:
                printf(" (Invalid Target Address)\n");
                break;
            case GW_FAIL_INTEGRITY:
                printf(" (CRC-8 Integrity Check Failed)\n");
                break;
            case GW_FAIL_LENGTH_MISMATCH:
                printf(" (Length Mismatch)\n");
                break;
            default:
                printf(" (Unknown Error)\n");
                break;
        }
    } else {
        printf("  Type: POSITIVE RESPONSE\n");
        printf("  Payload (%u byte%s, decrypted):",
               resp->payload_len, resp->payload_len == 1 ? "" : "s");
        for (uint32_t i = 0; i < resp->payload_len; i++) {
            printf(" 0x%02X", resp->decrypted_payload[i]);
        }
        printf("\n");
    }
    printf("---------------------------\n\n");
}
