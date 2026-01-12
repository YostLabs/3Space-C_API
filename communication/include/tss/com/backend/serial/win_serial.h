#ifndef __TSS_WIN_SERIAL_H__
#define __TSS_WIN_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

struct SerialDevice {
    void* handle;
    uint8_t port;

    OVERLAPPED overlap_read;
    OVERLAPPED overlap_write;

    uint32_t timeout;
    bool blocking;
};

#endif /* __TSS_WIN_SERIAL_H__ */
