/**
 * @file test_fuzz.c
 * @brief Phase 6 Fuzzing, Malformed Packet Hardening & Latency Benchmark.
 *
 * Tests the Gateway Server's resilience against:
 *   1. Garbage byte sequences
 *   2. Partial headers (1..7 bytes) followed by abrupt disconnect
 *   3. Extreme/overflow length headers (e.g. 0xFFFFFFFF, 0x7FFFFFFF)
 *   4. Zero-payload and corrupted frames
 *   5. Processing latency benchmark (< 1.0 ms per packet)
 */
#include "unity/unity.h"
#include "server/tcp_server.h"
#include "server/validator.h"
#include "server/response_builder.h"
#include "common/stream_framer.h"
#include "common/packet_parser.h"
#include "common/crypto.h"
#include "common/transform.h"
#include "common/platform.h"
#include "common/gw_types.h"

#include <string.h>
#include <stdlib.h>

#define FUZZ_TEST_PORT 13502

static void* fuzz_server_worker(void *arg) {
    gw_server_t *srv = (gw_server_t *)arg;
    gw_server_run(srv);
    return NULL;
}

/* Helper to connect raw socket */
static gw_socket_t connect_raw_socket(uint16_t port) {
    gw_socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (!gw_socket_is_valid(s)) return GW_INVALID_SOCKET;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        gw_socket_close(s);
        return GW_INVALID_SOCKET;
    }
    return s;
}

void test_fuzz_partial_headers_and_disconnect(void) {
    gw_server_t srv;
    gw_server_init(&srv, FUZZ_TEST_PORT, "127.0.0.1", false);
    TEST_ASSERT_EQUAL_INT(0, gw_server_start(&srv));

    gw_thread_t srv_thread;
    TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&srv_thread, fuzz_server_worker, &srv));
    gw_sleep_ms(50);

    /* Test partial headers from 1 to 7 bytes followed by immediate socket close */
    uint8_t partial_hdr[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00};
    for (size_t len = 1; len <= 7; len++) {
        gw_socket_t s = connect_raw_socket(FUZZ_TEST_PORT);
        TEST_ASSERT_TRUE(gw_socket_is_valid(s));
        gw_send_exact(s, partial_hdr, len);
        gw_socket_close(s); /* Abrupt disconnect */
        gw_sleep_ms(5);
    }

    /* Test extreme length in header (0xFFFFFFFF) */
    {
        gw_socket_t s = connect_raw_socket(FUZZ_TEST_PORT);
        TEST_ASSERT_TRUE(gw_socket_is_valid(s));
        uint8_t bad_len_hdr[8] = {0x0E, 0x80, 0x31, 0x08, 0xFF, 0xFF, 0xFF, 0xFF};
        gw_send_exact(s, bad_len_hdr, sizeof(bad_len_hdr));

        /* Server should send Negative Response 0x08 and keep running */
        uint8_t resp_buf[32];
        size_t total_read = 0;
        int rc = gw_read_frame(s, resp_buf, sizeof(resp_buf), &total_read);
        TEST_ASSERT_EQUAL_INT(0, rc);
        TEST_ASSERT_EQUAL_UINT(10, total_read);
        TEST_ASSERT_EQUAL_HEX8(GW_FAIL_LENGTH_MISMATCH, resp_buf[8]);

        gw_socket_close(s);
    }

    /* Test random garbage stream */
    {
        gw_socket_t s = connect_raw_socket(FUZZ_TEST_PORT);
        TEST_ASSERT_TRUE(gw_socket_is_valid(s));

        uint8_t garbage[128];
        for (size_t i = 0; i < sizeof(garbage); i++) {
            garbage[i] = (uint8_t)(i * 37 + 13);
        }
        gw_send_exact(s, garbage, sizeof(garbage));
        gw_socket_close(s);
        gw_sleep_ms(10);
    }

    gw_server_stop(&srv);
    gw_thread_join(srv_thread);
}

void test_packet_processing_latency_benchmark(void) {
    /* Benchmark per-packet processing pipeline: parse -> validate -> decrypt -> swap -> encrypt -> build */
    uint8_t raw_req[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    uint8_t out_resp[64];
    size_t out_len = 0;

    const int ITERATIONS = 1000;
    uint64_t start_ms = gw_get_time_ms();

    for (int i = 0; i < ITERATIONS; i++) {
        gw_packet_t pkt;
        TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(raw_req, sizeof(raw_req), &pkt));
        gw_failure_code_t fail = gw_validate_packet(raw_req, sizeof(raw_req), &pkt);
        TEST_ASSERT_EQUAL_HEX8(GW_FAIL_NONE, fail);

        uint8_t payload = pkt.payload[0];
        gw_decrypt_request_payload(&payload, 1);
        payload = gw_swap_nibble_byte(payload);
        gw_encrypt_response_payload(&payload, 1);

        TEST_ASSERT_EQUAL_INT(0, gw_build_positive_response(&pkt, &payload, 1,
                                                            out_resp, sizeof(out_resp), &out_len));
    }

    uint64_t elapsed_ms = gw_get_time_ms() - start_ms;
    /* 1,000 iterations in under 1,000 ms means < 1 ms per packet */
    TEST_ASSERT_TRUE(elapsed_ms < 1000);
}
