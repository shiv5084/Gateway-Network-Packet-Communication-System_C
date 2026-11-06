#include "common/packet_parser.h"
#include "common/platform.h"
#include <string.h>

int gw_parse_header(const uint8_t *buffer, size_t buf_len, gw_packet_t *pkt) {
    if (!buffer || buf_len < GW_HEADER_SIZE || !pkt) {
        return -1;
    }

    /* Extract big-endian 16-bit and 32-bit fields */
    uint16_t raw_sa;
    uint16_t raw_ta;
    uint32_t raw_len;

    memcpy(&raw_sa, buffer, sizeof(raw_sa));
    memcpy(&raw_ta, buffer + 2, sizeof(raw_ta));
    memcpy(&raw_len, buffer + 4, sizeof(raw_len));

    pkt->source_addr = gw_ntohs(raw_sa);
    pkt->target_addr = gw_ntohs(raw_ta);
    pkt->length      = gw_ntohl(raw_len);

    if (pkt->length >= 1) {
        pkt->payload_len = pkt->length - 1;
    } else {
        pkt->payload_len = 0;
    }

    pkt->payload = NULL;
    pkt->crc     = 0;

    return 0;
}

int gw_parse_packet(const uint8_t *buffer, size_t buf_len, gw_packet_t *pkt) {
    if (!buffer || !pkt) {
        return -1;
    }

    if (gw_parse_header(buffer, buf_len, pkt) != 0) {
        return -1;
    }

    if (pkt->length < 1) {
        return -3;
    }

    size_t expected_total = (size_t)GW_HEADER_SIZE + (size_t)pkt->length;
    if (buf_len < expected_total) {
        return -2;
    }

    /* Zero-copy payload assignment */
    if (pkt->payload_len > 0) {
        pkt->payload = buffer + GW_HEADER_SIZE;
    } else {
        pkt->payload = NULL;
    }

    /* CRC is located at offset: GW_HEADER_SIZE + length - 1 */
    pkt->crc = buffer[GW_HEADER_SIZE + pkt->length - 1];

    return 0;
}
