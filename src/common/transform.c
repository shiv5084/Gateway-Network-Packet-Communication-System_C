#include "common/transform.h"

void gw_swap_nibbles(uint8_t *data, size_t length) {
    if (!data) return;
    for (size_t i = 0; i < length; i++) {
        data[i] = gw_swap_nibble_byte(data[i]);
    }
}
