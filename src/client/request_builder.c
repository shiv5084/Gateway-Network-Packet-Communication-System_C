#include "client/request_builder.h"
#include "common/crc8.h"
#include "common/platform.h"
#include <string.h>

int gw_client_build_request(uint16_t sa, uint16_t ta,
                            const uint8_t *raw_payload, uint32_t payload_len,
                            uint8_t *out_buf, size_t max_out_len, size_t *out_len) {
    return gw_client_build_corrupt_request(sa, ta, raw_payload, payload_len,
                                           false, 0, out_buf, max_out_len, out_len);
}

int gw_client_build_corrupt_request(uint16_t sa, uint16_t ta,
                                    const uint8_t *raw_payload, uint32_t payload_len,
                                    bool corrupt_crc, uint32_t override_length,
                                    uint8_t *out_buf, size_t max_out_len, size_t *out_len) {
    if (!out_buf || !out_len) {
        return -1;
    }

    size_t needed_len = (size_t)GW_HEADER_SIZE + (size_t)payload_len + (size_t)GW_CRC_SIZE;
    if (max_out_len < needed_len) {
        return -1;
    }

    uint16_t sa_net = gw_htons(sa);
    uint16_t ta_net = gw_htons(ta);

    uint32_t length_val = (override_length > 0) ? override_length : (payload_len + 1);
    uint32_t length_net = gw_htonl(length_val);

    memcpy(out_buf, &sa_net, sizeof(sa_net));
    memcpy(out_buf + 2, &ta_net, sizeof(ta_net));
    memcpy(out_buf + 4, &length_net, sizeof(length_net));

    /* Encrypt payload bytes with XOR 0xAA */
    uint8_t *enc_payload = out_buf + GW_HEADER_SIZE;
    for (uint32_t i = 0; i < payload_len; i++) {
        enc_payload[i] = raw_payload[i] ^ GW_KEY_REQ_XOR;
    }

    /* Compute CRC-8 over encrypted payload */
    uint8_t crc = gw_crc8_default(enc_payload, payload_len);
    if (corrupt_crc) {
        crc ^= 0xFF; /* Invert CRC to trigger integrity failure */
    }

    out_buf[GW_HEADER_SIZE + payload_len] = crc;

    *out_len = needed_len;
    return 0;
}
