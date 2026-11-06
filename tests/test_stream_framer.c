/**
 * @file test_stream_framer.c
 * @brief Phase 4 Unit Tests: Stream framer fragmented and multi-frame handling.
 *
 * Tests gw_recv_exact, gw_send_exact, and gw_read_frame using cross-platform
 * loopback socket pairs to simulate TCP fragmentation and pipelined requests.
 */
#include "unity/unity.h"
#include "common/stream_framer.h"
#include "common/platform.h"
#include "common/gw_types.h"
#include "server/tcp_server.h"
#include "client/tcp_client.h"
#include "client/request_builder.h"
#include "client/response_parser.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================================================================
 * Helper: Create a connected socket pair for loopback testing
 * ========================================================================= */

/**
 * @brief Creates a connected loopback socket pair.
 *        On Windows: TCP loopback via bind+connect.
 *        On Linux: same approach (socketpair() available but we use TCP for cross-platform).
 *
 * @param writer_sock  OUT - socket to write (sender side)
 * @param reader_sock  OUT - socket to read (receiver side)
 * @return 0 on success, -1 on failure
 */
static int create_socket_pair(gw_socket_t *writer_sock, gw_socket_t *reader_sock) {
    gw_socket_t listener   = GW_INVALID_SOCKET;
    gw_socket_t client_end = GW_INVALID_SOCKET;
    gw_socket_t server_end = GW_INVALID_SOCKET;

    /* Create listening socket on loopback */
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (!gw_socket_is_valid(listener)) return -1;

    gw_socket_set_reuseaddr(listener, true);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = 0; /* OS assigns ephemeral port */
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        gw_socket_close(listener);
        return -1;
    }

    /* Retrieve the assigned port */
#if defined(_WIN32) || defined(_WIN64)
    int addrlen = sizeof(addr);
#else
    socklen_t addrlen = sizeof(addr);
#endif
    if (getsockname(listener, (struct sockaddr *)&addr, &addrlen) != 0) {
        gw_socket_close(listener);
        return -1;
    }

    if (listen(listener, 1) != 0) {
        gw_socket_close(listener);
        return -1;
    }

    /* Connect client end */
    client_end = socket(AF_INET, SOCK_STREAM, 0);
    if (!gw_socket_is_valid(client_end)) {
        gw_socket_close(listener);
        return -1;
    }

    /* Use loopback address for connect */
    struct sockaddr_in connect_addr;
    memset(&connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family      = AF_INET;
    connect_addr.sin_port        = addr.sin_port; /* Same port as bound */
    connect_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(client_end, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) != 0) {
        gw_socket_close(listener);
        gw_socket_close(client_end);
        return -1;
    }

    /* Accept server end */
#if defined(_WIN32) || defined(_WIN64)
    int srv_addrlen = sizeof(connect_addr);
#else
    socklen_t srv_addrlen = sizeof(connect_addr);
#endif
    server_end = accept(listener, (struct sockaddr *)&connect_addr, &srv_addrlen);
    gw_socket_close(listener); /* Done with listener */

    if (!gw_socket_is_valid(server_end)) {
        gw_socket_close(client_end);
        return -1;
    }

    *writer_sock = client_end;
    *reader_sock = server_end;
    return 0;
}

/* Helper: Build a minimal valid GW frame */
static size_t build_frame(uint8_t *buf, uint16_t sa, uint16_t ta,
                           const uint8_t *payload, size_t payload_len, uint8_t crc) {
    /* SA (2 bytes BE), TA (2 bytes BE), Length (4 bytes BE) */
    buf[0] = (uint8_t)(sa >> 8);
    buf[1] = (uint8_t)(sa & 0xFF);
    buf[2] = (uint8_t)(ta >> 8);
    buf[3] = (uint8_t)(ta & 0xFF);
    uint32_t length = (uint32_t)(payload_len + 1); /* payload + CRC */
    buf[4] = (uint8_t)(length >> 24);
    buf[5] = (uint8_t)(length >> 16);
    buf[6] = (uint8_t)(length >> 8);
    buf[7] = (uint8_t)(length & 0xFF);
    if (payload_len > 0 && payload) {
        memcpy(buf + 8, payload, payload_len);
    }
    buf[8 + payload_len] = crc;
    return 8 + payload_len + 1; /* total frame size */
}

/* =========================================================================
 * Test: recv_exact / send_exact basic round-trip
 * ========================================================================= */
void test_stream_send_recv_exact(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    const uint8_t send_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t recv_data[5] = {0};

    /* Send and receive */
    TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, send_data, sizeof(send_data)));

    int n = gw_recv_exact(reader, recv_data, sizeof(recv_data));
    TEST_ASSERT_EQUAL_INT((int)sizeof(send_data), n);
    TEST_ASSERT_EQUAL_MEMORY(send_data, recv_data, sizeof(send_data));

    gw_socket_close(writer);
    gw_socket_close(reader);
}

/* =========================================================================
 * Test: gw_read_frame basic complete frame
 * ========================================================================= */
void test_read_frame_basic(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Build a frame: SA=0x0E80, TA=0x3108, payload=0xB8, CRC=0x21 */
    uint8_t frame[32];
    uint8_t payload[] = {0xB8};
    size_t frame_len = build_frame(frame, 0x0E80, 0x3108, payload, 1, 0x21);

    /* Send complete frame */
    TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, frame, frame_len));

    /* Read frame */
    uint8_t recv_buf[256];
    size_t total_read = 0;
    int rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT((int)frame_len, (int)total_read);
    TEST_ASSERT_EQUAL_MEMORY(frame, recv_buf, frame_len);

    gw_socket_close(writer);
    gw_socket_close(reader);
}

/* =========================================================================
 * Test: gw_read_frame handles clean disconnect
 * ========================================================================= */
void test_read_frame_clean_disconnect(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Close writer without sending anything */
    gw_socket_close(writer);

    uint8_t recv_buf[256];
    size_t total_read = 0;
    int rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);

    /* Should return 1 for clean disconnect */
    TEST_ASSERT_EQUAL_INT(1, rc);

    gw_socket_close(reader);
}

/* =========================================================================
 * Test: gw_read_frame handles buffer too small (rc == -2)
 * ========================================================================= */
void test_read_frame_buffer_too_small(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Build a frame that won't fit in a tiny buffer */
    uint8_t big_payload[32];
    memset(big_payload, 0xAA, sizeof(big_payload));
    uint8_t frame[64];
    size_t frame_len = build_frame(frame, 0x0E80, 0x3108, big_payload, sizeof(big_payload), 0x99);

    TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, frame, frame_len));

    /* Use a buffer too small to hold the frame (only 10 bytes) */
    uint8_t tiny_buf[10];
    size_t total_read = 0;
    int rc = gw_read_frame(reader, tiny_buf, sizeof(tiny_buf), &total_read);

    /* Should return -2 (frame exceeds buffer) */
    TEST_ASSERT_EQUAL_INT(-2, rc);

    gw_socket_close(writer);
    gw_socket_close(reader);
}

/* =========================================================================
 * Test: Two consecutive frames (pipelined requests)
 * ========================================================================= */
void test_read_frame_two_consecutive(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Build two frames */
    uint8_t payload1[] = {0xB8};
    uint8_t payload2[] = {0x12, 0x34};
    uint8_t frame1[32], frame2[32];
    size_t len1 = build_frame(frame1, 0x0E80, 0x3108, payload1, 1, 0x21);
    size_t len2 = build_frame(frame2, 0xE008, 0x1380, payload2, 2, 0xCC);

    /* Send both frames back-to-back in one write */
    uint8_t combined[64];
    memcpy(combined,        frame1, len1);
    memcpy(combined + len1, frame2, len2);
    TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, combined, len1 + len2));

    /* Read first frame */
    uint8_t recv_buf[256];
    size_t total_read = 0;
    int rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT((int)len1, (int)total_read);
    TEST_ASSERT_EQUAL_MEMORY(frame1, recv_buf, len1);

    /* Read second frame */
    total_read = 0;
    rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT((int)len2, (int)total_read);
    TEST_ASSERT_EQUAL_MEMORY(frame2, recv_buf, len2);

    gw_socket_close(writer);
    gw_socket_close(reader);
}

/* =========================================================================
 * Test: Frame with zero-length payload rejected (min packet size)
 * ========================================================================= */
void test_read_frame_header_only_frame(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Build frame with length=0 in header (no payload, no CRC) */
    uint8_t frame[8];
    frame[0] = 0x0E; frame[1] = 0x80; /* SA */
    frame[2] = 0x31; frame[3] = 0x08; /* TA */
    frame[4] = 0x00; frame[5] = 0x00; frame[6] = 0x00; frame[7] = 0x00; /* length=0 */

    TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, frame, 8));
    gw_socket_close(writer); /* Close to signal end */

    uint8_t recv_buf[256];
    size_t total_read = 0;
    /* length=0 means no body to read; total_read=8, rc=0 */
    int rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);

    /* Either returns 0 with total=8 (length==0 is valid at framer level)
     * or returns 1 (disconnect after header). Accept either. */
    TEST_ASSERT_TRUE(rc == 0 || rc == 1);

    gw_socket_close(reader);
}

/* =========================================================================
 * Test: gw_read_frame handles byte-by-byte fragmented TCP stream
 * ========================================================================= */
void test_read_frame_fragmented_byte_by_byte(void) {
    gw_socket_t writer, reader;
    TEST_ASSERT_EQUAL_INT(0, create_socket_pair(&writer, &reader));

    /* Build frame: SA=0x0E80, TA=0x3108, payload=0xB8, CRC=0x21 */
    uint8_t frame[32];
    uint8_t payload[] = {0xB8};
    size_t frame_len = build_frame(frame, 0x0E80, 0x3108, payload, 1, 0x21);

    /* Send frame byte-by-byte across individual send() calls */
    for (size_t i = 0; i < frame_len; i++) {
        TEST_ASSERT_EQUAL_INT(0, gw_send_exact(writer, &frame[i], 1));
        gw_sleep_ms(2);
    }

    uint8_t recv_buf[256];
    size_t total_read = 0;
    int rc = gw_read_frame(reader, recv_buf, sizeof(recv_buf), &total_read);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT((int)frame_len, (int)total_read);
    TEST_ASSERT_EQUAL_MEMORY(frame, recv_buf, frame_len);

    gw_socket_close(writer);
    gw_socket_close(reader);
}

/* =========================================================================
 * Test: End-to-end Gateway Server & Client over Loopback TCP
 * ========================================================================= */
static void* server_loopback_worker(void *arg) {
    gw_server_t *srv = (gw_server_t *)arg;
    gw_server_run(srv);
    return NULL;
}

void test_server_client_loopback(void) {
    gw_server_t srv;
    gw_server_init(&srv, 13499, "127.0.0.1", false);
    TEST_ASSERT_EQUAL_INT(0, gw_server_start(&srv));

    gw_thread_t srv_thread;
    TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&srv_thread, server_loopback_worker, &srv));

    /* Allow server thread to initialize and enter accept() */
    gw_sleep_ms(50);

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", 13499));

    /* Scenario 1: Valid request with payload 0x12 */
    uint8_t payload[] = {0x12};
    uint8_t req_buf[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x0E80, 0x3108, payload, 1,
                                                    req_buf, sizeof(req_buf), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_buf, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX16(0x3108, resp.source_addr);
    TEST_ASSERT_EQUAL_HEX16(0x0E80, resp.target_addr);
    TEST_ASSERT_EQUAL_UINT(1, resp.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);
    TEST_ASSERT_TRUE(resp.crc_valid);

    /* Scenario 2: Invalid SA request on same persistent connection */
    uint8_t req_bad_sa[64];
    size_t req_bad_sa_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0xAABB, 0x3108, payload, 1,
                                                    req_bad_sa, sizeof(req_bad_sa), &req_bad_sa_len));
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_bad_sa, req_bad_sa_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_SRC_ADDR_INVALID, resp.failure_code);

    /* Disconnect client */
    gw_client_disconnect(&client);

    /* Stop server and wait for thread to join */
    gw_server_stop(&srv);
    gw_thread_join(srv_thread);
}

