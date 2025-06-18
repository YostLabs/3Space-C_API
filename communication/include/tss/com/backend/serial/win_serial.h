#ifndef __TSS_WIN_SERIAL_H__
#define __TSS_WIN_SERIAL_H__

#include <stdint.h>

struct SerialDevice {
    void* handle;
    uint8_t port;

    uint32_t timeout;
};

#endif /* __TSS_WIN_SERIAL_H__ */
