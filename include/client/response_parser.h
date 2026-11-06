#ifndef RESPONSE_PARSER_H
#define RESPONSE_PARSER_H

#include "common/gw_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gw_response_type_t type;            /**< GW_RESP_POSITIVE or GW_RESP_NEGATIVE */
    uint16_t           source_addr;     /**< Source address of response */
    uint16_t           target_addr;     /**< Target address of response */
    gw_failure_code_t  failure_code;    /**< Failure code (if negative response) */
    uint8_t            decrypted_payload[GW_MAX_PAYLOAD_SIZE]; /**< Decrypted payload (if positive) */
    uint32_t           payload_len;     /**< Length of decrypted payload */
    uint8_t            crc;             /**< Received CRC */
    bool               crc_valid;       /**< True if CRC verified successfully */
} gw_client_response_t;

/**
 * @brief Parses and decodes a response frame received from the Gateway server.
 *
 * @param buffer Raw response bytes received from server
 * @param buf_len Total response bytes available
 * @param resp Output decoded response structure
 * @return 0 on success, -1 on invalid format / truncated frame
 */
int gw_client_parse_response(const uint8_t *buffer, size_t buf_len,
                             gw_client_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* RESPONSE_PARSER_H */
