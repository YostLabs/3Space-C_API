#include "tss/com/spi.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

#include <stdbool.h>

static int spi_open(struct TSS_Com_Class *com);
static int spi_close(struct TSS_Com_Class *com);

static int spi_read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out);

static void spi_set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms);
static uint32_t spi_get_timeout(struct TSS_Com_Class *com);

static int spi_write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

static const struct TSS_Com_Class_API m_spi_com_api = {
    .open  = spi_open,
    .close = spi_close,

    // SPI devices are at fixed hardware addresses; reenumeration and
    // auto-detection are not applicable.
    .reenumerate = NULL,
    .auto_detect = NULL,

    .in = {
        .read       = spi_read,
        .read_until = tssManagedComBaseReadUntil,

        .set_timeout     = spi_set_timeout,
        .get_timeout     = spi_get_timeout,

        .clear_immediate = tssManagedComBaseClear,
        .clear_timeout   = tssManagedComBaseClearTimeout,
    },
    .out = {
        .write = spi_write,
    },
};

void create_spi_com_class(SpiPortId id, uint32_t speed_hz, struct SpiComClass *out)
{
    *out = (struct SpiComClass) {
        .device = {
            .id = id,
            .speed_hz = speed_hz,
        },
        .spi_com = (struct TSS_Com_Class) {
            .api          = &m_spi_com_api,
            .reenumerates = false,
        },
    };

    tssCreateManagedCom(
        &out->spi_com,
        (struct TSS_Com_Class *)out,
        out->read_buffer,  sizeof(out->read_buffer),
        out->write_buffer, sizeof(out->write_buffer),
        &out->base
    );
}

static int spi_open(struct TSS_Com_Class *com)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    return spiOpen(self->device.id, self->device.speed_hz, &self->device);
}

static int spi_close(struct TSS_Com_Class *com)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    spiClose(&self->device);
    return 0;
}

static int spi_read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    return spiRead(&self->device, num_bytes, out);
}

static void spi_set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    spiSetTimeout(&self->device, timeout_ms);
}

static uint32_t spi_get_timeout(struct TSS_Com_Class *com)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    return spiGetTimeout(&self->device);
}

static int spi_write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len)
{
    struct SpiComClass *self = (struct SpiComClass *)com;
    return spiWrite(&self->device, bytes, len);
}
