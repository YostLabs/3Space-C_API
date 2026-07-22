#ifndef __SPI_COM_CLASS_H__
#define __SPI_COM_CLASS_H__

#include "tss/com/managed_com.h"
#include "tss/com/backend/spi/spi_device.h"
#include "tss/utility/ring_buf2.h"
#include "tss/export.h"

struct SpiComClass {
    struct TSS_Managed_Com_Class base;
    struct TSS_Com_Class spi_com;
    struct SpiDevice device;

#if TSS_MINIMAL_SENSOR == 0
    uint8_t read_buffer[4096];
#endif

#if TSS_BUFFERED_WRITES
    uint8_t write_buffer[512];
#endif
};

/**
 * @brief Initialises a SpiComClass for the SPI device at the given port ID.
 * The device is not opened until the com class open() function is called.
 * @param id       SPI port ID (e.g. "/dev/spidev0.0" for linux).
 * @param speed_hz Clock frequency in Hz (e.g. 1000000 for 1 MHz).
 * @param out      Output struct to initialise. Must outlive all use of the com class.
 */
TSS_API void create_spi_com_class(SpiPortId id, uint32_t speed_hz, struct SpiComClass *out);

#endif /* __SPI_COM_CLASS_H__ */
