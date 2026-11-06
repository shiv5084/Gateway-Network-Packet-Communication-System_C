/**
 * @file server_metrics.h
 * @brief Thread-safe telemetry counters and metrics reporting for Gateway Server.
 */
#ifndef SERVER_METRICS_H
#define SERVER_METRICS_H

#include "common/gw_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Snapshot of server performance and telemetry counters.
 */
typedef struct {
    uint64_t total_connections;
    uint64_t total_requests_received;
    uint64_t positive_responses_sent;
    uint64_t negative_responses_sent;
    uint64_t fail_0x01_sa;
    uint64_t fail_0x02_ta;
    uint64_t fail_0x04_crc;
    uint64_t fail_0x08_len;
    uint64_t other_failures;
} gw_server_metrics_t;

/**
 * @brief Initializes the telemetry metrics subsystem and its synchronization primitives.
 */
void gw_metrics_init(void);

/**
 * @brief Increments accepted connection count.
 */
void gw_metrics_inc_connections(void);

/**
 * @brief Increments total requests received counter.
 */
void gw_metrics_inc_requests(void);

/**
 * @brief Increments positive response sent counter.
 */
void gw_metrics_inc_positive(void);

/**
 * @brief Increments negative response sent counter and specific failure code counter.
 *
 * @param code Failure code returned in negative response
 */
void gw_metrics_inc_negative(gw_failure_code_t code);

/**
 * @brief Retrieves a consistent snapshot of the current metrics counters.
 *
 * @param out_metrics Pointer to struct where snapshot will be written
 */
void gw_metrics_get(gw_server_metrics_t *out_metrics);

/**
 * @brief Prints formatted metrics summary to stdout and log.
 */
void gw_metrics_dump(void);

/**
 * @brief Resets all telemetry counters to zero (useful for tests).
 */
void gw_metrics_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_METRICS_H */
