/**
 * @file test_integration.c
 * @brief Phase 6 Integration Tests: Full loopback client-server functional test suite.
 */
#include "unity/unity.h"
#include "server/tcp_server.h"
#include "server/server_metrics.h"
#include "client/tcp_client.h"
#include "client/request_builder.h"
#include "client/response_parser.h"
#include "common/platform.h"
#include "common/gw_types.h"
#include "common/logger.h"

#include <string.h>

#define TEST_SERVER_PORT 13500

static gw_server_t s_int_server;
static gw_thread_t s_int_server_thread;
static bool s_int_server_running = false;

static void* integration_server_worker(void *arg) {
    gw_server_t *srv = (gw_server_t *)arg;
    gw_server_run(srv);
    return NULL;
}

static void start_test_server(void) {
    if (!s_int_server_running) {
        gw_metrics_reset();
        gw_server_init(&s_int_server, TEST_SERVER_PORT, "127.0.0.1", false);
        TEST_ASSERT_EQUAL_INT(0, gw_server_start(&s_int_server));
        TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&s_int_server_thread, integration_server_worker, &s_int_server));
        s_int_server_running = true;
        gw_sleep_ms(60);
    }
}

static void stop_test_server(void) {
    if (s_int_server_running) {
        gw_server_stop(&s_int_server);
        gw_thread_join(s_int_server_thread);
        s_int_server_running = false;
        gw_sleep_ms(30);
    }
}

/* Test Case 1: Valid 1-byte payload (0x12 -> 0x21) */
void test_integration_case1_valid_single_byte(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload[] = {0x12};
    uint8_t req[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x0E80, 0x3108, payload, 1, req, sizeof(req), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX16(0x3108, resp.source_addr);
    TEST_ASSERT_EQUAL_HEX16(0x0E80, resp.target_addr);
    TEST_ASSERT_EQUAL_UINT(1, resp.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);
    TEST_ASSERT_TRUE(resp.crc_valid);

    gw_client_disconnect(&client);
}

/* Test Case 2: Valid multi-byte payload (0x01, 0x02, 0x03, 0x04 -> 0x10, 0x20, 0x30, 0x40) */
void test_integration_case2_valid_multibyte(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t expected[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t req[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0xE008, 0x1380, payload, 4, req, sizeof(req), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX16(0x1380, resp.source_addr);
    TEST_ASSERT_EQUAL_HEX16(0xE008, resp.target_addr);
    TEST_ASSERT_EQUAL_UINT(4, resp.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp.decrypted_payload, 4);
    TEST_ASSERT_TRUE(resp.crc_valid);

    gw_client_disconnect(&client);
}

/* Test Case 3: Invalid SA 0x1122 -> Negative Response 0x01 */
void test_integration_case3_invalid_sa(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload[] = {0x12};
    uint8_t req[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x1122, 0x3108, payload, 1, req, sizeof(req), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_SRC_ADDR_INVALID, resp.failure_code);

    gw_client_disconnect(&client);
}

/* Test Case 4: Invalid TA 0x3344 -> Negative Response 0x02 */
void test_integration_case4_invalid_ta(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload[] = {0x12};
    uint8_t req[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x0E80, 0x3344, payload, 1, req, sizeof(req), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_TGT_ADDR_INVALID, resp.failure_code);

    gw_client_disconnect(&client);
}

/* Test Case 5: Corrupted CRC -> Negative Response 0x04 */
void test_integration_case5_corrupted_crc(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload[] = {0x12};
    uint8_t req[64];
    size_t req_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_corrupt_request(0x0E80, 0x3108, payload, 1,
                                                            true, 0, req, sizeof(req), &req_len));

    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, req_len, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_INTEGRITY, resp.failure_code);

    gw_client_disconnect(&client);
}

/* Test Case 6: Payload length mismatch -> Negative Response 0x08 */
void test_integration_case6_length_mismatch(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    /* Send frame with length=0 in header */
    uint8_t req[8] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x00};
    gw_client_response_t resp;
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req, sizeof(req), &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_LENGTH_MISMATCH, resp.failure_code);

    gw_client_disconnect(&client);
}

/* Test Case 7: Alternating valid and invalid requests on a single persistent connection */
void test_integration_case7_alternating_requests_persistent_conn(void) {
    start_test_server();

    gw_client_t client;
    gw_client_init(&client, false);
    TEST_ASSERT_EQUAL_INT(0, gw_client_connect(&client, "127.0.0.1", TEST_SERVER_PORT));

    uint8_t payload1[] = {0x12};
    uint8_t req_valid[64];
    size_t len_valid = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x0E80, 0x3108, payload1, 1,
                                                    req_valid, sizeof(req_valid), &len_valid));

    uint8_t req_bad_sa[64];
    size_t len_bad_sa = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x9999, 0x3108, payload1, 1,
                                                    req_bad_sa, sizeof(req_bad_sa), &len_bad_sa));

    uint8_t req_bad_crc[64];
    size_t len_bad_crc = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_client_build_corrupt_request(0x0E80, 0x3108, payload1, 1,
                                                            true, 0, req_bad_crc, sizeof(req_bad_crc), &len_bad_crc));

    gw_client_response_t resp;

    /* Request 1: Valid */
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_valid, len_valid, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);

    /* Request 2: Bad SA */
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_bad_sa, len_bad_sa, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_SRC_ADDR_INVALID, resp.failure_code);

    /* Request 3: Valid again */
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_valid, len_valid, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);

    /* Request 4: Bad CRC */
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_bad_crc, len_bad_crc, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_INTEGRITY, resp.failure_code);

    /* Request 5: Valid again */
    TEST_ASSERT_EQUAL_INT(0, gw_client_send_request(&client, req_valid, len_valid, &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);

    gw_client_disconnect(&client);

    /* Reconcile telemetry metrics */
    gw_server_metrics_t m;
    gw_metrics_get(&m);
    TEST_ASSERT_EQUAL_UINT((unsigned int)m.total_requests_received,
                           (unsigned int)(m.positive_responses_sent + m.negative_responses_sent));

    stop_test_server();
}
