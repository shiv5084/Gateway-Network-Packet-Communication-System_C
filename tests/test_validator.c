#include "unity/unity.h"
#include "common/gw_types.h"
#include "common/packet_parser.h"
#include "server/validator.h"
#include "server/response_builder.h"
#include <string.h>

void test_scenario_1_valid_request(void) {
    /* Scenario 1: Valid single byte request (0E 80 31 08 00 00 00 02 B8 21) */
    uint8_t req_bytes[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(req_bytes, sizeof(req_bytes), &pkt));
    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_NONE, fail);

    /* Generate Positive Response */
    uint8_t encrypted_resp_payload[] = {0x74};
    uint8_t resp_buf[32];
    size_t resp_len = 0;

    TEST_ASSERT_EQUAL_INT(0, gw_build_positive_response(&pkt, encrypted_resp_payload,
                                                        sizeof(encrypted_resp_payload),
                                                        resp_buf, sizeof(resp_buf), &resp_len));

    /* Expected positive response: 31 08 0E 80 00 00 00 02 74 4B */
    uint8_t expected[] = {0x31, 0x08, 0x0E, 0x80, 0x00, 0x00, 0x00, 0x02, 0x74, 0x4B};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), resp_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp_buf, sizeof(expected));
}

void test_scenario_2_invalid_source_address(void) {
    /* Scenario 2: SA = 0xAABB */
    uint8_t req_bytes[] = {0xAA, 0xBB, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(req_bytes, sizeof(req_bytes), &pkt));
    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_SRC_ADDR_INVALID, fail);

    /* Generate Negative Response */
    uint8_t resp_buf[32];
    size_t resp_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_build_negative_response(&pkt, fail, resp_buf, sizeof(resp_buf), &resp_len));

    /* Expected negative response: 31 08 AA BB 00 00 00 02 01 07 */
    uint8_t expected[] = {0x31, 0x08, 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x02, 0x01, 0x07};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), resp_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp_buf, sizeof(expected));
}

void test_scenario_3_invalid_target_address(void) {
    /* Scenario 3: TA = 0x9999 */
    uint8_t req_bytes[] = {0x0E, 0x80, 0x99, 0x99, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(req_bytes, sizeof(req_bytes), &pkt));
    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_TGT_ADDR_INVALID, fail);

    /* Generate Negative Response */
    uint8_t resp_buf[32];
    size_t resp_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_build_negative_response(&pkt, fail, resp_buf, sizeof(resp_buf), &resp_len));

    /* Expected negative response: 99 99 0E 80 00 00 00 02 02 0E */
    uint8_t expected[] = {0x99, 0x99, 0x0E, 0x80, 0x00, 0x00, 0x00, 0x02, 0x02, 0x0E};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), resp_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp_buf, sizeof(expected));
}

void test_scenario_4_integrity_failure(void) {
    /* Scenario 4: Corrupt CRC 0xFF instead of 0x21 */
    uint8_t req_bytes[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0xFF};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(req_bytes, sizeof(req_bytes), &pkt));
    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_INTEGRITY, fail);

    /* Generate Negative Response */
    uint8_t resp_buf[32];
    size_t resp_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_build_negative_response(&pkt, fail, resp_buf, sizeof(resp_buf), &resp_len));

    /* Expected negative response: 31 08 0E 80 00 00 00 02 04 1C */
    uint8_t expected[] = {0x31, 0x08, 0x0E, 0x80, 0x00, 0x00, 0x00, 0x02, 0x04, 0x1C};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), resp_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp_buf, sizeof(expected));
}

void test_scenario_5_length_mismatch(void) {
    /* Scenario 5: Declared length is 5, but total packet is only 10 bytes */
    uint8_t req_bytes[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x05, 0xB8, 0x21};
    gw_packet_t pkt;
    gw_parse_header(req_bytes, sizeof(req_bytes), &pkt);

    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_LENGTH_MISMATCH, fail);

    /* Generate Negative Response */
    uint8_t resp_buf[32];
    size_t resp_len = 0;
    TEST_ASSERT_EQUAL_INT(0, gw_build_negative_response(&pkt, fail, resp_buf, sizeof(resp_buf), &resp_len));

    /* Expected negative response: 31 08 0E 80 00 00 00 02 08 38 */
    uint8_t expected[] = {0x31, 0x08, 0x0E, 0x80, 0x00, 0x00, 0x00, 0x02, 0x08, 0x38};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), resp_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, resp_buf, sizeof(expected));
}

void test_precedence_hierarchy(void) {
    /* Frame has BOTH invalid length (Length=5, actual=10) AND invalid SA (0x9999).
     * Stage 1 (0x08) must take priority over Stage 2 (0x01)! */
    uint8_t req_bytes[] = {0x99, 0x99, 0x31, 0x08, 0x00, 0x00, 0x00, 0x05, 0xB8, 0x21};
    gw_packet_t pkt;
    gw_parse_header(req_bytes, sizeof(req_bytes), &pkt);

    gw_failure_code_t fail = gw_validate_packet(req_bytes, sizeof(req_bytes), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_LENGTH_MISMATCH, fail);

    /* Frame has valid length, but BOTH invalid SA (0x9999) AND corrupted CRC (0xFF).
     * Stage 2 (0x01) must take priority over Stage 4 (0x04)! */
    uint8_t req_bytes2[] = {0x99, 0x99, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0xFF};
    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(req_bytes2, sizeof(req_bytes2), &pkt));
    fail = gw_validate_packet(req_bytes2, sizeof(req_bytes2), &pkt);
    TEST_ASSERT_EQUAL_HEX8(GW_FAIL_SRC_ADDR_INVALID, fail);
}
