#include "unity/unity.h"
#include "common/platform.h"
#include <stdio.h>

/* Phase 1 test functions */
extern void test_types_and_constants(void);
extern void test_byte_order_conversions(void);
extern void test_time_and_sleep(void);
extern void test_mutex_and_threads(void);
extern void test_socket_abstraction(void);
extern void test_logger(void);

/* Phase 2 parser & client serialization tests */
extern void test_parse_header_valid(void);
extern void test_parse_header_too_short(void);
extern void test_parse_full_packet(void);
extern void test_parse_packet_buffer_truncated(void);
extern void test_client_build_request(void);
extern void test_client_parse_positive_response(void);
extern void test_client_parse_negative_response(void);

/* Phase 2 validator & server response tests */
extern void test_scenario_1_valid_request(void);
extern void test_scenario_2_invalid_source_address(void);
extern void test_scenario_3_invalid_target_address(void);
extern void test_scenario_4_integrity_failure(void);
extern void test_scenario_5_length_mismatch(void);
extern void test_precedence_hierarchy(void);

/* Phase 3 crypto & transform tests */
extern void test_crc8_known_vectors(void);
extern void test_crc8_lut_bitwise_equivalence(void);
extern void test_xor_crypto_roundtrip(void);
extern void test_nibble_swap(void);
extern void test_full_transformation_pipeline(void);

/* Phase 4 stream framer & network layer tests */
extern void test_stream_send_recv_exact(void);
extern void test_read_frame_basic(void);
extern void test_read_frame_clean_disconnect(void);
extern void test_read_frame_buffer_too_small(void);
extern void test_read_frame_two_consecutive(void);
extern void test_read_frame_header_only_frame(void);
extern void test_read_frame_fragmented_byte_by_byte(void);
extern void test_server_client_loopback(void);

/* Phase 6 integration, concurrency & hardening tests */
extern void test_integration_case1_valid_single_byte(void);
extern void test_integration_case2_valid_multibyte(void);
extern void test_integration_case3_invalid_sa(void);
extern void test_integration_case4_invalid_ta(void);
extern void test_integration_case5_corrupted_crc(void);
extern void test_integration_case6_length_mismatch(void);
extern void test_integration_case7_alternating_requests_persistent_conn(void);
extern void test_concurrent_stress_zero_drop(void);
extern void test_fuzz_partial_headers_and_disconnect(void);
extern void test_packet_processing_latency_benchmark(void);

int main(void) {
    if (gw_platform_init() != 0) {
        fprintf(stderr, "FATAL: Failed to initialize platform networking!\n");
        return 1;
    }

    UnityBegin("Gateway Protocol Master Test Suite (Phase 1 through Phase 6)");

    /* Phase 1 Suite */
    RUN_TEST(test_types_and_constants);
    RUN_TEST(test_byte_order_conversions);
    RUN_TEST(test_time_and_sleep);
    RUN_TEST(test_mutex_and_threads);
    RUN_TEST(test_socket_abstraction);
    RUN_TEST(test_logger);

    /* Phase 2: Parser & Client Builder Suite */
    RUN_TEST(test_parse_header_valid);
    RUN_TEST(test_parse_header_too_short);
    RUN_TEST(test_parse_full_packet);
    RUN_TEST(test_parse_packet_buffer_truncated);
    RUN_TEST(test_client_build_request);
    RUN_TEST(test_client_parse_positive_response);
    RUN_TEST(test_client_parse_negative_response);

    /* Phase 2: Validator & Server Response Suite */
    RUN_TEST(test_scenario_1_valid_request);
    RUN_TEST(test_scenario_2_invalid_source_address);
    RUN_TEST(test_scenario_3_invalid_target_address);
    RUN_TEST(test_scenario_4_integrity_failure);
    RUN_TEST(test_scenario_5_length_mismatch);
    RUN_TEST(test_precedence_hierarchy);

    /* Phase 3: Cryptography & Transformation Pipeline Suite */
    RUN_TEST(test_crc8_known_vectors);
    RUN_TEST(test_crc8_lut_bitwise_equivalence);
    RUN_TEST(test_xor_crypto_roundtrip);
    RUN_TEST(test_nibble_swap);
    RUN_TEST(test_full_transformation_pipeline);

    /* Phase 4: Network Stream Framer & TCP Loopback Suite */
    RUN_TEST(test_stream_send_recv_exact);
    RUN_TEST(test_read_frame_basic);
    RUN_TEST(test_read_frame_clean_disconnect);
    RUN_TEST(test_read_frame_buffer_too_small);
    RUN_TEST(test_read_frame_two_consecutive);
    RUN_TEST(test_read_frame_header_only_frame);
    RUN_TEST(test_read_frame_fragmented_byte_by_byte);
    RUN_TEST(test_server_client_loopback);

    /* Phase 6: End-to-End Integration Suite */
    RUN_TEST(test_integration_case1_valid_single_byte);
    RUN_TEST(test_integration_case2_valid_multibyte);
    RUN_TEST(test_integration_case3_invalid_sa);
    RUN_TEST(test_integration_case4_invalid_ta);
    RUN_TEST(test_integration_case5_corrupted_crc);
    RUN_TEST(test_integration_case6_length_mismatch);
    RUN_TEST(test_integration_case7_alternating_requests_persistent_conn);

    /* Phase 6: Multi-Client Concurrency Stress & Zero-Drop Suite */
    RUN_TEST(test_concurrent_stress_zero_drop);

    /* Phase 6: Hardening, Fuzzing & Latency Benchmark */
    RUN_TEST(test_fuzz_partial_headers_and_disconnect);
    RUN_TEST(test_packet_processing_latency_benchmark);

    int failures = UnityEnd();

    gw_platform_cleanup();

    return (failures == 0) ? 0 : 1;
}
