#ifndef RESPONSE_BUILDER_H
#define RESPONSE_BUILDER_H

#include "common/gw_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serializes a Positive Response frame into out_buf.
 *
 * Header:
 *  - Source Address = req_pkt->target_addr
 *  - Target Address = req_pkt->source_addr
 *  - Length = payload_len + 1
 * Body:
 *  - Encrypted Payload (XOR 0x55)
 *  - CRC-8 over the encrypted response payload
 *
 * @param req_pkt Request packet (used to invert addresses)
 * @param encrypted_payload Pre-encrypted response payload (XOR 0x55)
 * @param payload_len Number of payload bytes
 * @param out_buf Output buffer (min 8 + payload_len + 1 bytes)
 * @param max_out_len Capacity of out_buf
 * @param out_len Output pointer receiving total frame length
 * @return 0 on success, -1 on buffer overflow or NULL pointer
 */
int gw_build_positive_response(const gw_packet_t *req_pkt,
                               const uint8_t *encrypted_payload,
                               uint32_t payload_len,
                               uint8_t *out_buf,
                               size_t max_out_len,
                               size_t *out_len);

/**
 * @brief Serializes a Negative Response frame (always 10 bytes) into out_buf.
 *
 * Header:
 *  - Source Address = req_pkt->target_addr (or default 0x3108)
 *  - Target Address = req_pkt->source_addr (or default 0x0E80)
 *  - Length = 0x00000002
 * Body:
 *  - Failure Code (1 byte, unencrypted)
 *  - CRC-8 over the Failure Code byte (1 byte)
 *
 * @param req_pkt Request packet (or NULL for default fallback addresses)
 * @param fail_code Failure status code
 * @param out_buf Output buffer (min 10 bytes)
 * @param max_out_len Capacity of out_buf
 * @param out_len Output pointer receiving total frame length (10)
 * @return 0 on success, -1 on buffer overflow or NULL pointer
 */
int gw_build_negative_response(const gw_packet_t *req_pkt,
                               gw_failure_code_t fail_code,
                               uint8_t *out_buf,
                               size_t max_out_len,
                               size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* RESPONSE_BUILDER_H */
