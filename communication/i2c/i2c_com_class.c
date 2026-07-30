#include "tss/com/i2c.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

#include <stdbool.h>

static int i2c_open(struct TSS_Com_Class *com);
static int i2c_close(struct TSS_Com_Class *com);

static int i2c_read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out);

static void i2c_set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms);
static uint32_t i2c_get_timeout(struct TSS_Com_Class *com);

static int i2c_write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

static const struct TSS_Com_Class_API m_i2c_com_api = {
    .open  = i2c_open,
    .close = i2c_close,

    // I2C devices are at fixed hardware addresses; reenumeration and
    // auto-detection are not applicable.
    .reenumerate = NULL,
    .auto_detect = NULL,

    .in = {
        .read       = i2c_read,
        .read_until = tssManagedComBaseReadUntil,

        .set_timeout     = i2c_set_timeout,
        .get_timeout     = i2c_get_timeout,

        .clear_immediate = tssManagedComBaseClear,
        .clear_timeout   = tssManagedComBaseClearTimeout,
    },
    .out = {
        .write = i2c_write,
    },
};

void create_i2c_com_class(I2cPortId id, uint32_t speed_hz, struct I2cComClass *out)
{
    *out = (struct I2cComClass) {
        .i2c_com = (struct TSS_Com_Class) {
            .api          = &m_i2c_com_api,
            .reenumerates = false,
        },
        .device = {
            .id = id,
            .speed_hz = speed_hz,
        },
    };

    tssCreateManagedCom(
        &out->i2c_com,
        (struct TSS_Com_Class *)out,
        out->read_buffer,  sizeof(out->read_buffer),
        out->write_buffer, sizeof(out->write_buffer),
        &out->base
    );
}

static int i2c_open(struct TSS_Com_Class *com)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    return i2cOpen(self->device.id, self->device.speed_hz, &self->device);
}

static int i2c_close(struct TSS_Com_Class *com)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    i2cClose(&self->device);
    return 0;
}

static int i2c_read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    return i2cRead(&self->device, num_bytes, out);
}

static void i2c_set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    i2cSetTimeout(&self->device, timeout_ms);
}

static uint32_t i2c_get_timeout(struct TSS_Com_Class *com)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    return i2cGetTimeout(&self->device);
}

static int i2c_write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len)
{
    struct I2cComClass *self = (struct I2cComClass *)com;
    return i2cWrite(&self->device, bytes, len);
}
