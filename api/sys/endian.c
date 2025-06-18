#include "tss/sys/endian.h"

void tssSwapEndianess(void *data, uint16_t p_size) {
    uint8_t i, tmp, *buf;
    buf = data;
    for(i = 0; i < p_size / 2; i++) {
        tmp = buf[i];
        buf[i] = buf[p_size-1-i];
        buf[p_size-1-i] = tmp;
    }
}