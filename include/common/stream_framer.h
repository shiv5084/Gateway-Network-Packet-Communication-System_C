#ifndef STREAM_FRAMER_H
#define STREAM_FRAMER_H

#include "common/gw_types.h"
#include "common/platform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads exactly `count` bytes from socket, handling partial reads.
 *
 * @param sock Active socket
 * @param buffer Output buffer (min `count` bytes)
 * @param count Number of bytes to receive
 * @return Number of bytes read (equal to count), 0 on clean disconnect, -1 on socket error.
 */
int gw_recv_exact(gw_socket_t sock, uint8_t *buffer, size_t count);

/**
 * @brief Sends all `count` bytes across a socket, handling partial writes.
 *
 * @param sock Active socket
 * @param buffer Data to send
 * @param count Number of bytes to send
 * @return 0 on success, -1 on error.
 */
int gw_send_exact(gw_socket_t sock, const uint8_t *buffer, size_t count);

/**
 * @brief Reads one complete Gateway packet using the two-phase framing protocol.
 *
 * Phase 1: Reads 8-byte header -> extracts 32-bit Length field.
 * Phase 2: Validates Length bounds -> reads exactly Length bytes (payload + CRC).
 *
 * @param sock Connected TCP socket
 * @param buffer Output buffer to receive complete frame
 * @param max_buf_len Capacity of output buffer
 * @param total_read Output pointer receiving total frame length (8 + Length)
 * @return 0 on success,
 *         1 on clean client disconnect,
 *        -1 on socket read error,
 *        -2 on frame size exceeding max_buf_len.
 */
int gw_read_frame(gw_socket_t sock, uint8_t *buffer, size_t max_buf_len, size_t *total_read);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_FRAMER_H */
