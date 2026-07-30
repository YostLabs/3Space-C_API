#ifndef __TSS_LINUX_I2C_H__
#define __TSS_LINUX_I2C_H__

#include <stdint.h>
#include <gpiod.h>

/*
* device_name, chip_path, and cs_line_num are required to create a SpiDevice.
* data_available_line_num and data_loaded_line_num are optional, but if provided, will be
* used to configure the data_available_line and data_loaded_line GPIO lines for the device
* and the read_fn.
*/
struct I2cDeviceInfo {
    char *device_name;
    uint8_t bus_address; // 7-bit I2C address of the device

    // Optional
    char *chip_path;
    int data_available_line_num;
    int data_loaded_line_num;
};

/*
* When creating a new I2cDevice, you must provide the device_name, 
*/
typedef struct I2cDeviceInfo I2cPortId;

struct I2cDevice {
    struct I2cDeviceInfo id;

    //File Descriptor for I2C Device
    int fd;

    //Optional IRQ pins for I2C protocol with a yostlabs sensor.
    struct gpiod_chip *chip;
    struct gpiod_line *data_available_line;
    struct gpiod_line *data_loaded_line;

    //Configuration parameters for the I2C device
    uint32_t speed_hz;

    // Timeout used by i2cRead (milliseconds). 0 = non-blocking.
    uint32_t timeout;
    //Timeout for specifically the header portion of a Transactional Response
    uint32_t header_timeout;

    // Read strategy used by i2cRead. Swap this pointer to change the
    // chunked-read behaviour without altering any higher-level code.
    // Default (set by i2cOpen): i2cReadNoIrq.
    int (*read_fn)(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);
};

#endif /* __TSS_LINUX_I2C_H__ */
