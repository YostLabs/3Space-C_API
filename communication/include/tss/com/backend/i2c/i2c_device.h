#ifndef __TSS_I2C_DEVICE_H__
#define __TSS_I2C_DEVICE_H__

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
/* TODO: Add a platform-specific header for a USB-to-SPI converter backend on Windows */
#elif defined(__linux__) || defined(unix)
#include "tss/com/backend/i2c/linux_i2c.h"
#endif

/**
 * @brief Opens the I2C device at /dev/i2c-<bus> and configures it.
 * Sets the default read strategy (i2cReadNoIrq) on the device.
 * @return 0 on success, non-zero on error.
 */
int i2cOpen(I2cPortId id, uint32_t speed_hz, struct I2cDevice *out);

/**
 * @brief Closes the I2C device, releasing any held resources.
 */
void i2cClose(struct I2cDevice *dev);

/**
 * @brief Writes @p len bytes to the I2C device.
 * @return 0 on success, non-zero on error.
 */
int i2cWrite(struct I2cDevice *dev, const uint8_t *data, size_t len);

/**
 * @brief High-level read that calls dev->read_fn in 255-byte chunks until
 * @p num_bytes are received or the timeout stored in the device (dev->timeout) expires.
 * @return Total bytes received, or negative on error.
 */
int i2cRead(struct I2cDevice *dev, size_t num_bytes, uint8_t *out);

/** @return Current timeout in milliseconds (0 = non-blocking). */
uint32_t i2cGetTimeout(const struct I2cDevice *dev);

/** @brief Sets the timeout used by i2cRead (milliseconds; 0 = non-blocking). */
void i2cSetTimeout(struct I2cDevice *dev, uint32_t timeout_ms);

/**
 * @brief Optionally sets up pins to utilize the data available and data loaded GPIO IRQ lines from the sensor.
 * This is not required for operation, but may improve performance and reduce CPU usage by allowing the
 * host to wait for a signal rather than repeatedly polling the sensor for data. This is primarily advantageous
 * when expecting low data rates from the sensor. At high data rates, then sensor will almost always have
 * data available, so additionally checking the IRQ lines is not necessary and may even reduce performance.
 * @param data_available_line_num GPIO pin number for the Data Available line from the sensor. Set to -1 to disable.
 * @param data_loaded_line_num GPIO pin number for the Data Loaded line from the sensor. Set to -1 to disable.
 * @note The best read function will be set based on the provided pins. In general, provide None, just data_available, or both.
 * @return 0 on success, non-zero on error.
 */
int i2cConfigurePinMode(struct I2cDevice *dev, int data_available_line_num, int data_loaded_line_num);

#endif /* __TSS_I2C_DEVICE_H__ */
