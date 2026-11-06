#include "server/response_builder.h"
#include "common/crc8.h"
#include "common/platform.h"
#include <string.h>

int gw_build_positive_response(const gw_packet_t *req_pkt,
                               const uint8_t *encrypted_payload,
                               uint32_t payload_len,
                               uint8_t *out_buf,
                               size_t max_out_len,
                               size_t *out_len) {
    if (!req_pkt || !out_buf || !out_len) {
        return -1;
    }

    size_t needed_len = (size_t)GW_HEADER_SIZE + (size_t)payload_len + (size_t)GW_CRC_SIZE;
    if (max_out_len < needed_len) {
        return -1;
    }

    /* Invert addresses: Server sends from Request TA to Request SA */
    uint16_t sa = gw_htons(req_pkt->target_addr);
    uint16_t ta = gw_htons(req_pkt->source_addr);
    uint32_t length_val = payload_len + 1;
    uint32_t length_net = gw_htonl(length_val);

    memcpy(out_buf, &sa, sizeof(sa));
    memcpy(out_buf + 2, &ta, sizeof(ta));
    memcpy(out_buf + 4, &length_net, sizeof(length_net));

    if (payload_len > 0 && encrypted_payload) {
        memcpy(out_buf + GW_HEADER_SIZE, encrypted_payload, payload_len);
    }

    /* Compute CRC over encrypted response payload */
    uint8_t crc = gw_crc8_default(encrypted_payload, payload_len);
    out_buf[GW_HEADER_SIZE + payload_len] = crc;

    *out_len = needed_len;
    return 0;
}

int gw_build_negative_response(const gw_packet_t *req_pkt,
                               gw_failure_code_t fail_code,
                               uint8_t *out_buf,
                               size_t max_out_len,
                               size_t *out_len) {
    if (!out_buf || !out_len) {
        return -1;
    }

    const size_t needed_len = 10; /* 8 header + 1 failure code + 1 CRC */
    if (max_out_len < needed_len) {
        return -1;
    }

    /* Address handling: echo target to source, or use canonical fallback */
    uint16_t sa_val = req_pkt ? req_pkt->target_addr : GW_ADDR_GW_1_REQ_TA;
    uint16_t ta_val = req_pkt ? req_pkt->source_addr : GW_ADDR_NODE_1_REQ_SA;

    uint16_t sa = gw_htons(sa_val);
    uint16_t ta = gw_htons(ta_val);
    uint32_t length_net = gw_htonl(2); /* 1 byte failure code + 1 byte CRC */

    memcpy(out_buf, &sa, sizeof(sa));
    memcpy(out_buf + 2, &ta, sizeof(ta));
    memcpy(out_buf + 4, &length_net, sizeof(length_net));

    /* Unencrypted failure code */
    uint8_t code_byte = (uint8_t)fail_code;
    out_buf[GW_HEADER_SIZE] = code_byte;

    /* CRC over unencrypted failure code byte */
    uint8_t crc = gw_crc8_default(&code_byte, 1);
    out_buf[GW_HEADER_SIZE + 1] = crc;

    *out_len = needed_len;
    return 0;
}
