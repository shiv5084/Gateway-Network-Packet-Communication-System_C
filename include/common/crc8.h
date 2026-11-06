#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes CRC-8 checksum over a byte buffer using bitwise calculation.
 *
 * Parameters:
 *  - Polynomial: 0x07 (x^8 + x^2 + x + 1)
 *  - Initial value: 0x00
 *  - Reflect In: False
 *  - Reflect Out: False
 *  - Final XOR: 0x00
 *
 * @param data Pointer to input data buffer
 * @param length Number of bytes in buffer
 * @param poly CRC-8 polynomial (default: GW_CRC8_POLYNOMIAL / 0x07)
 * @param init Initial CRC value (default: GW_CRC8_INITIAL_VAL / 0x00)
 * @return uint8_t Calculated CRC-8 checksum
 */
uint8_t gw_crc8(const uint8_t *data, size_t length, uint8_t poly, uint8_t init);

/**
 * @brief Computes CRC-8 using fast lookup-table (LUT) accelerated calculation.
 *
 * @param data Pointer to input data buffer
 * @param length Number of bytes
 * @return uint8_t Calculated CRC-8 checksum
 */
uint8_t gw_crc8_lut(const uint8_t *data, size_t length);

/**
 * @brief Computes CRC-8 using default protocol parameters (poly=0x07, init=0x00).
 */
static inline uint8_t gw_crc8_default(const uint8_t *data, size_t length) {
    return gw_crc8_lut(data, length);
}

#ifdef __cplusplus
}
#endif

#endif /* CRC8_H */
