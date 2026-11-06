#include "unity/unity.h"
#include "common/crc8.h"
#include "common/crypto.h"
#include "common/transform.h"
#include <string.h>

void test_crc8_known_vectors(void) {
    /* Test empty buffer */
    TEST_ASSERT_EQUAL_HEX8(0x00, gw_crc8(NULL, 0, 0x07, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x00, gw_crc8_lut(NULL, 0));

    /* Test single byte [0xB8] -> 0x21 */
    uint8_t b1[] = {0xB8};
    TEST_ASSERT_EQUAL_HEX8(0x21, gw_crc8(b1, 1, 0x07, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x21, gw_crc8_lut(b1, 1));

    /* Test single byte [0x74] -> 0x4B */
    uint8_t b2[] = {0x74};
    TEST_ASSERT_EQUAL_HEX8(0x4B, gw_crc8(b2, 1, 0x07, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x4B, gw_crc8_lut(b2, 1));

    /* Test failure code bytes */
    uint8_t f1[] = {0x01};
    TEST_ASSERT_EQUAL_HEX8(0x07, gw_crc8_lut(f1, 1));
    uint8_t f2[] = {0x02};
    TEST_ASSERT_EQUAL_HEX8(0x0E, gw_crc8_lut(f2, 1));
    uint8_t f3[] = {0x04};
    TEST_ASSERT_EQUAL_HEX8(0x1C, gw_crc8_lut(f3, 1));
    uint8_t f4[] = {0x08};
    TEST_ASSERT_EQUAL_HEX8(0x38, gw_crc8_lut(f4, 1));
}

void test_crc8_lut_bitwise_equivalence(void) {
    /* Verify all 256 byte values produce exact same result in LUT vs Bitwise */
    for (int i = 0; i < 256; i++) {
        uint8_t byte = (uint8_t)i;
        uint8_t bitwise = gw_crc8(&byte, 1, 0x07, 0x00);
        uint8_t lut = gw_crc8_lut(&byte, 1);
        TEST_ASSERT_EQUAL_HEX8(bitwise, lut);
    }
}

void test_xor_crypto_roundtrip(void) {
    uint8_t original[] = {0x12, 0x34, 0xAB, 0xCD, 0x00, 0xFF};
    uint8_t buffer[6];
    memcpy(buffer, original, sizeof(original));

    /* Encrypt with 0xAA */
    gw_decrypt_request_payload(buffer, sizeof(buffer)); /* XOR 0xAA */
    TEST_ASSERT_FALSE(memcmp(original, buffer, sizeof(original)) == 0);

    /* Decrypt with 0xAA */
    gw_decrypt_request_payload(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_MEMORY(original, buffer, sizeof(original));

    /* Encrypt with 0x55 */
    gw_encrypt_response_payload(buffer, sizeof(buffer)); /* XOR 0x55 */
    TEST_ASSERT_FALSE(memcmp(original, buffer, sizeof(original)) == 0);

    /* Decrypt with 0x55 */
    gw_encrypt_response_payload(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_MEMORY(original, buffer, sizeof(original));
}

void test_nibble_swap(void) {
    TEST_ASSERT_EQUAL_HEX8(0x21, gw_swap_nibble_byte(0x12));
    TEST_ASSERT_EQUAL_HEX8(0xA3, gw_swap_nibble_byte(0x3A));
    TEST_ASSERT_EQUAL_HEX8(0x00, gw_swap_nibble_byte(0x00));
    TEST_ASSERT_EQUAL_HEX8(0xFF, gw_swap_nibble_byte(0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x5A, gw_swap_nibble_byte(0xA5));

    uint8_t buffer[] = {0x12, 0x3A, 0x00, 0xFF, 0xA5};
    uint8_t expected[] = {0x21, 0xA3, 0x00, 0xFF, 0x5A};

    gw_swap_nibbles(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));
}

void test_full_transformation_pipeline(void) {
    /* End-to-end trace from Problem Statement: 0x12 -> 0xB8 -> 0x12 -> 0x21 -> 0x74 -> 0x21 */
    uint8_t payload = 0x12;

    /* 1. Client encrypts: 0x12 ^ 0xAA = 0xB8 */
    uint8_t wire_req = payload ^ 0xAA;
    TEST_ASSERT_EQUAL_HEX8(0xB8, wire_req);

    /* 2. Server decrypts: 0xB8 ^ 0xAA = 0x12 */
    uint8_t gw_decrypted = wire_req ^ 0xAA;
    TEST_ASSERT_EQUAL_HEX8(0x12, gw_decrypted);

    /* 3. Server swaps nibbles: 0x12 -> 0x21 */
    uint8_t gw_swapped = gw_swap_nibble_byte(gw_decrypted);
    TEST_ASSERT_EQUAL_HEX8(0x21, gw_swapped);

    /* 4. Server encrypts: 0x21 ^ 0x55 = 0x74 */
    uint8_t wire_resp = gw_swapped ^ 0x55;
    TEST_ASSERT_EQUAL_HEX8(0x74, wire_resp);

    /* 5. Client decrypts: 0x74 ^ 0x55 = 0x21 */
    uint8_t client_final = wire_resp ^ 0x55;
    TEST_ASSERT_EQUAL_HEX8(0x21, client_final);
}
