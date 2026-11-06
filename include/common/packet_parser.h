#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include "common/gw_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses only the 8-byte header from a buffer into a gw_packet_t.
 *        Does NOT validate address validity or CRC; extracts field values using Big-Endian conversion.
 *
 * @param buffer Raw byte buffer
 * @param buf_len Length of available bytes in buffer (must be at least 8 bytes)
 * @param pkt Output parsed packet structure
 * @return 0 on success, -1 if buffer is NULL or shorter than GW_HEADER_SIZE
 */
int gw_parse_header(const uint8_t *buffer, size_t buf_len, gw_packet_t *pkt);

/**
 * @brief Parses a complete packet (header + payload pointer + CRC).
 *
 * @param buffer Raw byte buffer
 * @param buf_len Total available bytes in buffer
 * @param pkt Output parsed packet structure
 * @return 0 on success,
 *        -1 if buffer is too short for header (< 8),
 *        -2 if total buffer length is less than 8 + pkt->length,
 *        -3 if pkt->length < 1 (no CRC byte).
 */
int gw_parse_packet(const uint8_t *buffer, size_t buf_len, gw_packet_t *pkt);

#ifdef __cplusplus
}
#endif

#endif /* PACKET_PARSER_H */
