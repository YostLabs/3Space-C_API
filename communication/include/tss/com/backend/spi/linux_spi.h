#ifndef __TSS_LINUX_SPI_H__
#define __TSS_LINUX_SPI_H__

#include <stdint.h>
#include <gpiod.h>

struct SpiDeviceInfo {
    char *device_name;
    char *chip_path;
    unsigned int cs_line_num;
};

/*
* When creating a new SpiDevice, you must provide the device_name, 
*/
typedef struct SpiDeviceInfo SpiPortId;

struct SpiDevice {
    struct SpiDeviceInfo id;

    //File Descriptor for SPI Device
    int fd;

    //Manual Chip Select (Required because Linux SPI driver does not wait long enough after asserting CS before reading the first byte)
    struct gpiod_chip *chip;
    struct gpiod_line *cs_line;

    //Configuration parameters for the SPI device
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
