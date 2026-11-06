#include "client/response_parser.h"
#include "common/packet_parser.h"
#include "common/crc8.h"
#include <string.h>

int gw_client_parse_response(const uint8_t *buffer, size_t buf_len,
                             gw_client_response_t *resp) {
    if (!buffer || buf_len < GW_MIN_PACKET_SIZE || !resp) {
        return -1;
    }

    gw_packet_t pkt;
    if (gw_parse_packet(buffer, buf_len, &pkt) != 0) {
        return -1;
    }

    memset(resp, 0, sizeof(*resp));
    resp->source_addr = pkt.source_addr;
    resp->target_addr = pkt.target_addr;
    resp->crc         = pkt.crc;

    /* Verify CRC-8 over payload field */
    uint8_t calculated_crc;
    if (pkt.payload_len > 0) {
        calculated_crc = gw_crc8_default(pkt.payload, pkt.payload_len);
    } else {
        calculated_crc = gw_crc8_default(NULL, 0);
    }
    resp->crc_valid = (calculated_crc == pkt.crc);

    /*
     * Negative response is indicated by Length == 2 AND a single failure code byte
     * matching one of the defined protocol failure codes (0x01, 0x02, 0x04, 0x08).
     */
    if (pkt.length == 2 && pkt.payload_len == 1 &&
        (pkt.payload[0] == GW_FAIL_SRC_ADDR_INVALID ||
         pkt.payload[0] == GW_FAIL_TGT_ADDR_INVALID ||
         pkt.payload[0] == GW_FAIL_INTEGRITY ||
         pkt.payload[0] == GW_FAIL_LENGTH_MISMATCH)) {
        resp->type = GW_RESP_NEGATIVE;
        resp->failure_code = (gw_failure_code_t)pkt.payload[0];
        resp->payload_len = 0;
    } else {
        resp->type = GW_RESP_POSITIVE;
        resp->failure_code = GW_FAIL_NONE;
        resp->payload_len = pkt.payload_len;

        /* Decrypt response payload with XOR 0x55 */
        for (uint32_t i = 0; i < pkt.payload_len; i++) {
            resp->decrypted_payload[i] = pkt.payload[i] ^ GW_KEY_RESP_XOR;
        }
    }

    return 0;
}
