#include "server/validator.h"
#include "common/crc8.h"

gw_failure_code_t gw_validate_packet(const uint8_t *buffer, size_t total_len,
                                     const gw_packet_t *pkt) {
    /* -------------------------------------------------------------------------
     * Stage 1: Length Verification (0x08)
     * Total packet must be at least 9 bytes (8 header + 1 CRC).
     * Declared length must match actual bytes received: total_len == 8 + length.
     * Declared length must be >= 1 (at least 1 byte CRC).
     * ------------------------------------------------------------------------- */
    if (!buffer || total_len < GW_MIN_PACKET_SIZE || !pkt) {
        return GW_FAIL_LENGTH_MISMATCH;
    }

    if (pkt->length < 1) {
        return GW_FAIL_LENGTH_MISMATCH;
    }

    if (total_len != ((size_t)GW_HEADER_SIZE + (size_t)pkt->length)) {
        return GW_FAIL_LENGTH_MISMATCH;
    }

    /* -------------------------------------------------------------------------
     * Stage 2: Source Address Check (0x01)
     * Must be 0x0E80 or 0xE008
     * ------------------------------------------------------------------------- */
    if (pkt->source_addr != GW_ADDR_NODE_1_REQ_SA &&
        pkt->source_addr != GW_ADDR_NODE_2_REQ_SA) {
        return GW_FAIL_SRC_ADDR_INVALID;
    }

    /* -------------------------------------------------------------------------
     * Stage 3: Target Address Check (0x02)
     * Must be 0x3108 or 0x1380
     * ------------------------------------------------------------------------- */
    if (pkt->target_addr != GW_ADDR_GW_1_REQ_TA &&
        pkt->target_addr != GW_ADDR_GW_2_REQ_TA) {
        return GW_FAIL_TGT_ADDR_INVALID;
    }

    /* -------------------------------------------------------------------------
     * Stage 4: Integrity Verification / CRC-8 (0x04)
     * Calculated CRC over encrypted payload must match the received CRC byte.
     * If payload_len is 0, CRC is calculated over empty buffer (0x00).
     * ------------------------------------------------------------------------- */
    uint8_t calculated_crc = gw_crc8_default(pkt->payload, pkt->payload_len);
    if (calculated_crc != pkt->crc) {
        return GW_FAIL_INTEGRITY;
    }

    return GW_FAIL_NONE;
}
