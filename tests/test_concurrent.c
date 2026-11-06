/**
 * @file test_concurrent.c
 * @brief Phase 6 Concurrency & Zero-Drop Stress Test.
 *
 * Spawns multiple concurrent client threads connecting simultaneously to the
 * Gateway Server and streaming alternating valid and invalid requests.
 * Verifies zero-drop policy: total responses == total requests sent.
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
#include <stdlib.h>

#define CONCURRENT_TEST_PORT 13501
#define NUM_CLIENT_THREADS   8
#define REQUESTS_PER_CLIENT  50

typedef struct {
    int thread_id;
    int success_count;
    int fail_count;
} client_worker_arg_t;

static void* concurrent_client_worker(void *arg) {
    client_worker_arg_t *carg = (client_worker_arg_t *)arg;
    gw_client_t client;
    gw_client_init(&client, false);

    if (gw_client_connect(&client, "127.0.0.1", CONCURRENT_TEST_PORT) != 0) {
        carg->fail_count = REQUESTS_PER_CLIENT;
        return NULL;
    }

    uint8_t payload[] = {0x12};
    uint8_t req_valid[64];
    size_t len_valid = 0;
    gw_client_build_request(0x0E80, 0x3108, payload, 1, req_valid, sizeof(req_valid), &len_valid);

    uint8_t req_bad[64];
    size_t len_bad = 0;
    gw_client_build_request(0x9999, 0x3108, payload, 1, req_bad, sizeof(req_bad), &len_bad);

    for (int i = 0; i < REQUESTS_PER_CLIENT; i++) {
        gw_client_response_t resp;
        int rc;
        if (i % 2 == 0) {
            rc = gw_client_send_request(&client, req_valid, len_valid, &resp);
            if (rc == 0 && resp.type == GW_RESP_POSITIVE && resp.decrypted_payload[0] == 0x21) {
                carg->success_count++;
            } else {
                carg->fail_count++;
            }
        } else {
            rc = gw_client_send_request(&client, req_bad, len_bad, &resp);
            if (rc == 0 && resp.type == GW_RESP_NEGATIVE && resp.failure_code == GW_FAIL_SRC_ADDR_INVALID) {
                carg->success_count++;
            } else {
                carg->fail_count++;
            }
        }
    }

    gw_client_disconnect(&client);
    return NULL;
}

static void* server_concurrent_runner(void *arg) {
    gw_server_t *srv = (gw_server_t *)arg;
    gw_server_run(srv);
    return NULL;
}

void test_concurrent_stress_zero_drop(void) {
    gw_metrics_reset();

    gw_server_t server;
    gw_server_init(&server, CONCURRENT_TEST_PORT, "127.0.0.1", false);
    TEST_ASSERT_EQUAL_INT(0, gw_server_start(&server));

    gw_thread_t srv_thread;
    TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&srv_thread, server_concurrent_runner, &server));

    gw_sleep_ms(60); /* Allow server to bind and listen */

    gw_thread_t client_threads[NUM_CLIENT_THREADS];
    client_worker_arg_t args[NUM_CLIENT_THREADS];
    memset(args, 0, sizeof(args));

    /* Spawn all client threads concurrently */
    for (int i = 0; i < NUM_CLIENT_THREADS; i++) {
        args[i].thread_id = i;
        TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&client_threads[i], concurrent_client_worker, &args[i]));
    }

    /* Wait for all client threads to complete */
    int total_client_success = 0;
    int total_client_fail = 0;
    for (int i = 0; i < NUM_CLIENT_THREADS; i++) {
        gw_thread_join(client_threads[i]);
        total_client_success += args[i].success_count;
        total_client_fail += args[i].fail_count;
    }

    int expected_total_requests = NUM_CLIENT_THREADS * REQUESTS_PER_CLIENT;
    TEST_ASSERT_EQUAL_INT(0, total_client_fail);
    TEST_ASSERT_EQUAL_INT(expected_total_requests, total_client_success);

    /* Stop server */
    gw_server_stop(&server);
    gw_thread_join(srv_thread);

    /* Zero-Drop Policy Verification: Telemetry Reconciliation */
    gw_server_metrics_t m;
    gw_metrics_get(&m);
    TEST_ASSERT_EQUAL_UINT((unsigned int)expected_total_requests, (unsigned int)m.total_requests_received);
    TEST_ASSERT_EQUAL_UINT((unsigned int)m.total_requests_received,
                           (unsigned int)(m.positive_responses_sent + m.negative_responses_sent));
}
