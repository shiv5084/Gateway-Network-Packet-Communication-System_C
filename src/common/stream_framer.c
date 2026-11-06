#include "common/stream_framer.h"
#include <string.h>

int gw_recv_exact(gw_socket_t sock, uint8_t *buffer, size_t count) {
    if (!gw_socket_is_valid(sock) || !buffer) {
        return -1;
    }

    size_t total_received = 0;
    while (total_received < count) {
        size_t remaining = count - total_received;
#if defined(_WIN32) || defined(_WIN64)
        int n = recv(sock, (char*)(buffer + total_received), (int)remaining, 0);
#else
        ssize_t n = recv(sock, buffer + total_received, remaining, 0);
#endif
        if (n == 0) {
            /* Connection cleanly closed by peer */
            if (total_received == 0) {
                return 0;
            }
            return -1; /* Unexpected EOF mid-packet */
        }
        if (n < 0) {
            return -1; /* Socket read error */
        }
        total_received += (size_t)n;
    }

    return (int)total_received;
}

int gw_send_exact(gw_socket_t sock, const uint8_t *buffer, size_t count) {
    if (!gw_socket_is_valid(sock) || !buffer) {
        return -1;
    }

    size_t total_sent = 0;
    while (total_sent < count) {
        size_t remaining = count - total_sent;
#if defined(_WIN32) || defined(_WIN64)
        int n = send(sock, (const char*)(buffer + total_sent), (int)remaining, 0);
#else
        ssize_t n = send(sock, buffer + total_sent, remaining, MSG_NOSIGNAL);
#endif
        if (n <= 0) {
            return -1;
        }
        total_sent += (size_t)n;
    }

    return 0;
}

int gw_read_frame(gw_socket_t sock, uint8_t *buffer, size_t max_buf_len, size_t *total_read) {
    if (!gw_socket_is_valid(sock) || !buffer || !total_read || max_buf_len < GW_MIN_PACKET_SIZE) {
        return -1;
    }

    /* Phase 1: Read exactly 8-byte header */
    int res = gw_recv_exact(sock, buffer, GW_HEADER_SIZE);
    if (res == 0) {
        return 1; /* Clean peer disconnect */
    }
    if (res < 0) {
        return -1; /* Read error */
    }

    /* Extract 32-bit Length field (network byte order) */
    uint32_t raw_len;
    memcpy(&raw_len, buffer + 4, sizeof(raw_len));
    uint32_t length = gw_ntohl(raw_len);

    /* Phase 2: Validate length against buffer capacity (prevent overflow and exhaustion) */
    if (length > (uint32_t)(max_buf_len - GW_HEADER_SIZE) ||
        length > (uint32_t)(GW_MAX_PAYLOAD_SIZE + 1)) {
        return -2; /* Frame exceeds buffer capacity or protocol maximum */
    }

    size_t total_frame_len = (size_t)GW_HEADER_SIZE + (size_t)length;

    if (length > 0) {
        res = gw_recv_exact(sock, buffer + GW_HEADER_SIZE, length);
        if (res <= 0) {
            return -1; /* Truncated or closed connection mid-body */
        }
    }

    *total_read = total_frame_len;
    return 0;
}
