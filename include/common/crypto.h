#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Performs in-place XOR transformation on a data buffer.
 *
 * @param data Pointer to buffer to transform
 * @param length Number of bytes
 * @param key XOR key byte (e.g. 0xAA or 0x55)
 */
void gw_xor_buffer(uint8_t *data, size_t length, uint8_t key);

/**
 * @brief Decrypts incoming client request payload (in-place XOR with 0xAA).
 */
void gw_decrypt_request_payload(uint8_t *payload, size_t length);

/**
 * @brief Encrypts outgoing server response payload (in-place XOR with 0x55).
 */
void gw_encrypt_response_payload(uint8_t *payload, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
