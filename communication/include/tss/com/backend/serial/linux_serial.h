#ifndef __TSS_LINUX_SERIAL_H__
#define __TSS_LINUX_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>

// Port encoding:
//   port 0-127   -> /dev/ttyUSB<port>
//   port 128-255 -> /dev/ttyACM<port - 128>
struct SerialDevice {
    int fd;
    uint8_t port;
    uint32_t timeout;
    bool blocking;
};

#endif /* __TSS_LINUX_SERIAL_H__ */
