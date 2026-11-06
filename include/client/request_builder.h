#ifndef REQUEST_BUILDER_H
#define REQUEST_BUILDER_H

#include "common/gw_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Constructs a valid Client Request frame.
 *        Encrypts raw payload with XOR 0xAA, computes CRC-8, and packs header.
 *
 * @param sa Source address (e.g. GW_ADDR_NODE_1_REQ_SA / 0x0E80)
 * @param ta Target address (e.g. GW_ADDR_GW_1_REQ_TA / 0x3108)
 * @param raw_payload Unencrypted raw payload bytes (can be NULL if len == 0)
 * @param payload_len Number of raw payload bytes
 * @param out_buf Output buffer
 * @param max_out_len Size of out_buf (min 8 + payload_len + 1)
 * @param out_len Output: total frame bytes written
 * @return 0 on success, -1 on buffer overflow or NULL pointer
 */
int gw_client_build_request(uint16_t sa, uint16_t ta,
                            const uint8_t *raw_payload, uint32_t payload_len,
                            uint8_t *out_buf, size_t max_out_len, size_t *out_len);

/**
 * @brief Constructs a Client Request with optional intentional defects for negative testing.
 *
 * @param sa Source address to embed
 * @param ta Target address to embed
 * @param raw_payload Raw payload bytes
 * @param payload_len Number of payload bytes
 * @param corrupt_crc If true, corrupts CRC byte (forces 0x04)
 * @param override_length If non-zero, overrides length header field (e.g. forces 0x08)
 * @param out_buf Output buffer
 * @param max_out_len Buffer capacity
 * @param out_len Output: total bytes written
 * @return 0 on success, -1 on error
 */
int gw_client_build_corrupt_request(uint16_t sa, uint16_t ta,
                                    const uint8_t *raw_payload, uint32_t payload_len,
                                    bool corrupt_crc, uint32_t override_length,
                                    uint8_t *out_buf, size_t max_out_len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* REQUEST_BUILDER_H */
