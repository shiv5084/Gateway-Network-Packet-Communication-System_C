#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "common/gw_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Evaluates an incoming packet frame against the 4-stage validation hierarchy.
 *
 * Precedence Order:
 *  1. Length Check          -> GW_FAIL_LENGTH_MISMATCH (0x08)
 *  2. Source Address Check  -> GW_FAIL_SRC_ADDR_INVALID (0x01)
 *  3. Target Address Check  -> GW_FAIL_TGT_ADDR_INVALID (0x02)
 *  4. CRC-8 Integrity Check -> GW_FAIL_INTEGRITY (0x04)
 *
 * @param buffer Raw buffer received from TCP stream
 * @param total_len Actual byte count received
 * @param pkt Parsed packet structure (or NULL if header could not be parsed)
 * @return gw_failure_code_t (GW_FAIL_NONE if valid, or corresponding failure code)
 */
gw_failure_code_t gw_validate_packet(const uint8_t *buffer, size_t total_len,
                                     const gw_packet_t *pkt);

#ifdef __cplusplus
}
#endif

#endif /* VALIDATOR_H */
