/**
 * @file server_metrics.c
 * @brief Implementation of thread-safe telemetry metrics for Gateway Server.
 */
#include "server/server_metrics.h"
#include "common/platform.h"
#include "common/logger.h"

#include <stdio.h>
#include <string.h>

static gw_server_metrics_t s_metrics;
static gw_mutex_t s_metrics_mutex;
static bool s_metrics_initialized = false;

void gw_metrics_init(void) {
    if (!s_metrics_initialized) {
        gw_mutex_init(&s_metrics_mutex);
        memset(&s_metrics, 0, sizeof(s_metrics));
        s_metrics_initialized = true;
    }
}

void gw_metrics_inc_connections(void) {
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    s_metrics.total_connections++;
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_inc_requests(void) {
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    s_metrics.total_requests_received++;
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_inc_positive(void) {
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    s_metrics.positive_responses_sent++;
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_inc_negative(gw_failure_code_t code) {
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    s_metrics.negative_responses_sent++;
    switch (code) {
        case GW_FAIL_SRC_ADDR_INVALID:
            s_metrics.fail_0x01_sa++;
            break;
        case GW_FAIL_TGT_ADDR_INVALID:
            s_metrics.fail_0x02_ta++;
            break;
        case GW_FAIL_INTEGRITY:
            s_metrics.fail_0x04_crc++;
            break;
        case GW_FAIL_LENGTH_MISMATCH:
            s_metrics.fail_0x08_len++;
            break;
        default:
            s_metrics.other_failures++;
            break;
    }
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_get(gw_server_metrics_t *out_metrics) {
    if (!out_metrics) return;
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    memcpy(out_metrics, &s_metrics, sizeof(s_metrics));
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_reset(void) {
    if (!s_metrics_initialized) gw_metrics_init();
    gw_mutex_lock(&s_metrics_mutex);
    memset(&s_metrics, 0, sizeof(s_metrics));
    gw_mutex_unlock(&s_metrics_mutex);
}

void gw_metrics_dump(void) {
    gw_server_metrics_t m;
    gw_metrics_get(&m);

    printf("\n============================================================\n");
    printf(" Gateway Server Telemetry & Metrics Summary\n");
    printf("============================================================\n");
    printf("  Total Accepted Connections:     %llu\n", (unsigned long long)m.total_connections);
    printf("  Total Requests Received:        %llu\n", (unsigned long long)m.total_requests_received);
    printf("  Positive Responses (Success):   %llu\n", (unsigned long long)m.positive_responses_sent);
    printf("  Negative Responses (Rejected):  %llu\n", (unsigned long long)m.negative_responses_sent);
    printf("  ----------------------------------------------------------\n");
    printf("  Failure Code Breakdown:\n");
    printf("    - [0x01] Invalid Source Address: %llu\n", (unsigned long long)m.fail_0x01_sa);
    printf("    - [0x02] Invalid Target Address: %llu\n", (unsigned long long)m.fail_0x02_ta);
    printf("    - [0x04] CRC-8 Integrity Check:  %llu\n", (unsigned long long)m.fail_0x04_crc);
    printf("    - [0x08] Length Mismatch:        %llu\n", (unsigned long long)m.fail_0x08_len);
    if (m.other_failures > 0) {
        printf("    - Other / Protocol Failures:     %llu\n", (unsigned long long)m.other_failures);
    }
    printf("  Reconciliation Check: %s (Req: %llu == Pos+Neg: %llu)\n",
           (m.total_requests_received == m.positive_responses_sent + m.negative_responses_sent) ? "OK" : "MISMATCH",
           (unsigned long long)m.total_requests_received,
           (unsigned long long)(m.positive_responses_sent + m.negative_responses_sent));
    printf("============================================================\n\n");

    GW_LOG_I("METRICS", "Dumped metrics: Conn=%llu, Req=%llu, Pos=%llu, Neg=%llu",
             (unsigned long long)m.total_connections,
             (unsigned long long)m.total_requests_received,
             (unsigned long long)m.positive_responses_sent,
             (unsigned long long)m.negative_responses_sent);
}
