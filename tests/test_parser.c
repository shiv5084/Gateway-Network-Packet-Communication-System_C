#include "unity/unity.h"
#include "common/gw_types.h"
#include "common/packet_parser.h"
#include "client/request_builder.h"
#include "client/response_parser.h"
#include <string.h>

void test_parse_header_valid(void) {
    uint8_t buffer[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_header(buffer, sizeof(buffer), &pkt));
    TEST_ASSERT_EQUAL_HEX16(0x0E80, pkt.source_addr);
    TEST_ASSERT_EQUAL_HEX16(0x3108, pkt.target_addr);
    TEST_ASSERT_EQUAL_UINT(2, pkt.length);
    TEST_ASSERT_EQUAL_UINT(1, pkt.payload_len);
}

void test_parse_header_too_short(void) {
    uint8_t buffer[] = {0x0E, 0x80, 0x31, 0x08};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(-1, gw_parse_header(buffer, sizeof(buffer), &pkt));
}

void test_parse_full_packet(void) {
    uint8_t buffer[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(0, gw_parse_packet(buffer, sizeof(buffer), &pkt));
    TEST_ASSERT_NOT_NULL(pkt.payload);
    TEST_ASSERT_EQUAL_HEX8(0xB8, pkt.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, pkt.crc);
}

void test_parse_packet_buffer_truncated(void) {
    /* Header declares length 5 (expecting 13 bytes total), but only 10 bytes provided */
    uint8_t buffer[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x05, 0xB8, 0x21};
    gw_packet_t pkt;

    TEST_ASSERT_EQUAL_INT(-2, gw_parse_packet(buffer, sizeof(buffer), &pkt));
}

void test_client_build_request(void) {
    uint8_t raw_payload[] = {0x12};
    uint8_t out_buf[32];
    size_t out_len = 0;

    TEST_ASSERT_EQUAL_INT(0, gw_client_build_request(0x0E80, 0x3108,
                                                     raw_payload, sizeof(raw_payload),
                                                     out_buf, sizeof(out_buf), &out_len));

    /* Scenario 1 request vector: 0E 80 31 08 00 00 00 02 B8 21 */
    uint8_t expected[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x21};
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), out_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, out_buf, sizeof(expected));
}

void test_client_parse_positive_response(void) {
    /* Expected positive response: 31 08 0E 80 00 00 00 02 74 4B */
    uint8_t resp_bytes[] = {0x31, 0x08, 0x0E, 0x80, 0x00, 0x00, 0x00, 0x02, 0x74, 0x4B};
    gw_client_response_t resp;

    TEST_ASSERT_EQUAL_INT(0, gw_client_parse_response(resp_bytes, sizeof(resp_bytes), &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_POSITIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX16(0x3108, resp.source_addr);
    TEST_ASSERT_EQUAL_HEX16(0x0E80, resp.target_addr);
    TEST_ASSERT_TRUE(resp.crc_valid);
    TEST_ASSERT_EQUAL_UINT(1, resp.payload_len);
    /* 0x74 ^ 0x55 = 0x21 */
    TEST_ASSERT_EQUAL_HEX8(0x21, resp.decrypted_payload[0]);
}

void test_client_parse_negative_response(void) {
    /* Negative response for 0x01: 31 08 AA BB 00 00 00 02 01 07 */
    uint8_t resp_bytes[] = {0x31, 0x08, 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x02, 0x01, 0x07};
    gw_client_response_t resp;

    TEST_ASSERT_EQUAL_INT(0, gw_client_parse_response(resp_bytes, sizeof(resp_bytes), &resp));
    TEST_ASSERT_EQUAL_INT(GW_RESP_NEGATIVE, resp.type);
    TEST_ASSERT_EQUAL_HEX8(0x01, resp.failure_code);
    TEST_ASSERT_TRUE(resp.crc_valid);
}
