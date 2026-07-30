#ifndef __I2C_COM_CLASS_H__
#define __I2C_COM_CLASS_H__

#include "tss/com/managed_com.h"
#include "tss/com/backend/i2c/i2c_device.h"
#include "tss/utility/ring_buf2.h"
#include "tss/export.h"

struct I2cComClass {
    struct TSS_Managed_Com_Class base;
    struct TSS_Com_Class i2c_com;
    struct I2cDevice device;

#if TSS_MINIMAL_SENSOR == 0
    uint8_t read_buffer[4096];
#endif

#if TSS_BUFFERED_WRITES
    uint8_t write_buffer[512];
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialises an I2cComClass for the I2C device at the given port ID.
 * The device is not opened until the com class open() function is called.
 * @param id       I2C port ID (e.g. "/dev/i2c-1" for linux).
 * @param speed_hz Clock frequency in Hz (e.g. 1000000 for 1 MHz).
 * @param out      Output struct to initialise. Must outlive all use of the com class.
 */
TSS_API void create_i2c_com_class(I2cPortId id, uint32_t speed_hz, struct I2cComClass *out);

#ifdef __cplusplus
}
#endif
#endif /* __I2C_COM_CLASS_H__ */
