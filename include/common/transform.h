#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Swaps the high and low nibbles (4 bits each) for a single byte.
 *        Example: 0x12 -> 0x21, 0x3A -> 0xA3.
 *
 * @param byte Input byte
 * @return uint8_t Transformed byte with swapped nibbles
 */
static inline uint8_t gw_swap_nibble_byte(uint8_t byte) {
    return (uint8_t)(((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4));
}

/**
 * @brief Swaps the high and low nibbles of every byte in a buffer (in-place).
 *
 * @param data Pointer to buffer
 * @param length Number of bytes
 */
void gw_swap_nibbles(uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* TRANSFORM_H */
