#ifndef __TSS_SPI_DEVICE_H__
#define __TSS_SPI_DEVICE_H__

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
/* TODO: Add a platform-specific header for a USB-to-SPI converter backend on Windows */
#elif defined(__linux__) || defined(unix)
#include "tss/com/backend/spi/linux_spi.h"
#endif

/**
 * @brief Opens the SPI device at /dev/spidev<bus>.<cs> and configures it.
 * Sets the default read strategy (spiReadNoIrq) on the device.
 * @return 0 on success, non-zero on error.
 */
int spiOpen(SpiPortId id, uint32_t speed_hz, struct SpiDevice *out);

/**
 * @brief Closes the SPI device, releasing any held resources.
 */
void spiClose(struct SpiDevice *dev);

/**
 * @brief Basic write function. Raw write that adds no protocol bytes. Single transaction.
 * @param data Pointer to the data to write.
 * @param len Number of bytes to write.
 * @return 0 on success, non-zero on error.
 */
int spiBasicWrite(struct SpiDevice *dev, const uint8_t *data, size_t len);

/**
 * @brief Writes @p len bytes to the SPI device (TX only, RX discarded).
 * @return 0 on success, non-zero on error.
 */
int spiWrite(struct SpiDevice *dev, const uint8_t *data, size_t len);

/**
 * @brief Basic read function. Raw read, does not handle sending protocol read request. Single transaction.
 * @return Number of bytes received, or negative on error.
 */
int spiBasicRead(struct SpiDevice *dev, uint8_t *out, size_t len);

/**
 * @brief Full protocol read using the READ_DATA_WITH_SIZE command.
 * Sends the read command, polls the status byte until the sensor signals
 * that data is ready, then reads the payload.
 * @param length  Number of bytes to request from the sensor (max 255).
 * @param timeout_ms Maximum time to spend waiting for a valid response (ms).
 * @return Number of bytes received, or negative on error/timeout.
 */
int spiReadNoIrq(struct SpiDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);

/**
 * @brief High-level read that calls dev->read_fn in 255-byte chunks until
 * @p num_bytes are received or the timeout stored in the device (dev->timeout) expires.
 * @return Total bytes received, or negative on error.
 */
int spiRead(struct SpiDevice *dev, size_t num_bytes, uint8_t *out);

/** @return Current timeout in milliseconds (0 = non-blocking). */
uint32_t spiGetTimeout(struct SpiDevice *dev);

/** @brief Sets the timeout used by spiRead (milliseconds; 0 = non-blocking). */
void spiSetTimeout(struct SpiDevice *dev, uint32_t timeout_ms);

#endif /* __TSS_SPI_DEVICE_H__ */
