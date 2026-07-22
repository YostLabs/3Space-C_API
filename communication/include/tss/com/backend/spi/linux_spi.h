#ifndef __TSS_LINUX_SPI_H__
#define __TSS_LINUX_SPI_H__

#include <stdint.h>

typedef char* SpiPortId;

struct SpiDevice {
    SpiPortId port_id;
    
    int fd;
    uint32_t speed_hz;
    uint8_t bits_per_word;
    uint8_t mode;

    // Timeout used by spiRead (milliseconds). 0 = non-blocking.
    uint32_t timeout;
    //Timeout for specifically the header portion of a Transactional Response
    uint32_t header_timeout;

    // Read strategy used by spiRead. Swap this pointer to change the
    // chunked-read behaviour without altering any higher-level code.
    // Default (set by spiOpen): spiReadNoIrq.
    int (*read_fn)(struct SpiDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);
};

#endif /* __TSS_LINUX_SPI_H__ */
