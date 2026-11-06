#include "common/crypto.h"
#include "common/gw_types.h"

void gw_xor_buffer(uint8_t *data, size_t length, uint8_t key) {
    if (!data) return;
    for (size_t i = 0; i < length; i++) {
        data[i] ^= key;
    }
}

void gw_decrypt_request_payload(uint8_t *payload, size_t length) {
    gw_xor_buffer(payload, length, GW_KEY_REQ_XOR);
}

void gw_encrypt_response_payload(uint8_t *payload, size_t length) {
    gw_xor_buffer(payload, length, GW_KEY_RESP_XOR);
}
