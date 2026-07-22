#if defined(__linux__) || defined(unix)

#include "tss/com/backend/spi/spi_device.h"
#include "tss/constants.h"
#include "tss/sys/time.h"
#include "tss/errors.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

// -----------------------------------------------------------------------
// Open / Close
// -----------------------------------------------------------------------

int spiOpen(SpiPortId id, uint32_t speed_hz, struct SpiDevice *out)
{
    *out = (struct SpiDevice) {
        .id = id,
        .fd            = -1,
        .speed_hz      = speed_hz,
        .bits_per_word = 8,
        .mode          = SPI_MODE_0 | SPI_NO_CS,
        .timeout       = 1000,
        .header_timeout= 1,
        .read_fn       = spiReadNoIrq,
    };

    //----------------Configure GPIO CS----------------
    //Grab the chip
    out->chip = gpiod_chip_open(id.chip_path);
    if(!out->chip) {
        perror("gpiod_chip_open");
        return -1;
    }   

    //Grab the pin
    out->cs_line = gpiod_chip_get_line(out->chip, id.cs_line_num);
    if(!out->cs_line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(out->chip);
        out->chip = NULL;
        return -1;
    }

    //Configure the pin
    if(gpiod_line_request_output(out->cs_line, "spi_cs", 1) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(out->chip);
        out->chip = NULL;
        return -1;
    }

    //----------------Configure SPI device----------------

    int fd = open(id.device_name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open SPI device %s: %s\n", id.device_name, strerror(errno));
        gpiod_line_release(out->cs_line);
        gpiod_chip_close(out->chip);
        out->cs_line = NULL;
        out->chip = NULL;
        return -1;
    }

    out->fd = fd;

    if (ioctl(fd, SPI_IOC_WR_MODE,           &out->mode)          < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD,  &out->bits_per_word) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ,   &out->speed_hz)      < 0) 
        {
            close(fd);
            gpiod_line_release(out->cs_line);
            gpiod_chip_close(out->chip);
            out->cs_line = NULL;
            out->chip = NULL;
            out->fd = -1;
            return -1;
        }
    return 0;
}

void spiClose(struct SpiDevice *dev)
{
    if (dev->fd < 0) return;
    close(dev->fd);
    gpiod_line_release(dev->cs_line);
    gpiod_chip_close(dev->chip);
    dev->fd = -1;
}

// -----------------------------------------------------------------------
// Write
// -----------------------------------------------------------------------

int spiBasicWrite(struct SpiDevice *dev, const uint8_t *data, size_t len)
{
    if (len == 0 || dev->fd < 0) return 0;

    struct spi_ioc_transfer xfer = {
        .tx_buf        = (unsigned long)data,
        .rx_buf        = 0,
        .len           = (uint32_t)len,
        .speed_hz      = dev->speed_hz,
        .bits_per_word = dev->bits_per_word,
    };

    if (ioctl(dev->fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        perror("spiBasicWrite: SPI_IOC_MESSAGE");
        return -1;
    }
    return 0;
}

int spiWrite(struct SpiDevice *dev, const uint8_t *data, size_t len)
{
    if (len == 0 || dev->fd < 0) return 0;
    // To avoid a memcopy, creating two transaction objects. One for the Write Header
    // and one for the data.
    uint8_t write_header[2] = { TSS_TRANSACTION_WRITE_DATA_BYTE, 0xFF };
    struct spi_ioc_transfer xfer[2] = { 
        { .rx_buf = 0, .speed_hz = dev->speed_hz, .bits_per_word = dev->bits_per_word, .cs_change = 0 },
        { .rx_buf = 0, .speed_hz = dev->speed_hz, .bits_per_word = dev->bits_per_word },
    };

    gpiod_line_set_value(dev->cs_line, 0); // Set CS low
    uint8_t send_len = 0;
    while(len > 0) {
        send_len = (len > 255) ? 255 : len;
        write_header[1] = send_len;
        
        xfer[0].tx_buf = (unsigned long)write_header;
        xfer[0].len    = sizeof(write_header);
        xfer[1].tx_buf = (unsigned long)data;
        xfer[1].len    = send_len;
        if (ioctl(dev->fd, SPI_IOC_MESSAGE(2), xfer) < 0) {
            gpiod_line_set_value(dev->cs_line, 1); // Set CS high
            perror("spiWrite: SPI_IOC_MESSAGE");
            return -1;
        }

        len -= send_len;
        data += send_len;
    }
    gpiod_line_set_value(dev->cs_line, 1); // Set CS high
    return 0;
}

// -----------------------------------------------------------------------
// Basic read
// -----------------------------------------------------------------------

int spiBasicRead(struct SpiDevice *dev, uint8_t *out, size_t len)
{
    if (len == 0 || dev->fd < 0) return 0;

    struct spi_ioc_transfer xfer = {
        .tx_buf        = 0,
        .rx_buf        = (unsigned long)out,
        .len           = (uint32_t)len,
        .speed_hz      = dev->speed_hz,
        .bits_per_word = dev->bits_per_word,
    };

    if (ioctl(dev->fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        perror("spiBasicRead: SPI_IOC_MESSAGE");
        return -1;
    }
    return (int)len;
}

// -----------------------------------------------------------------------
// Protocol read (no-IRQ polling style)
// -----------------------------------------------------------------------

int spiReadNoIrq(struct SpiDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms)
{
    if (length == 0) return 0;
    // Send READ_DATA_WITH_SIZE command followed by the requested byte count.
    uint8_t header[2] = { TSS_TRANSACTION_READ_DATA_WITH_SIZE_BYTE, length };
    gpiod_line_set_value(dev->cs_line, 0); // Set CS low
    spiBasicWrite(dev, header, sizeof(header));
    gpiod_line_set_value(dev->cs_line, 1); // Set CS high

    uint8_t status = 0xFF, data_len = 0;
    tss_time_t start = tssTimeGet();
    uint32_t elapsed_time = 0;
    while (status == 0xFF && elapsed_time <= dev->header_timeout) {
        //Doing in this order to ensure a toggle between iterations, and that it stays low after the while loop.
        gpiod_line_set_value(dev->cs_line, 1); // Set CS high
        gpiod_line_set_value(dev->cs_line, 0); // Set CS low

        uint32_t remaining = timeout_ms - elapsed_time;
        memset(header, 0xFF, sizeof(header));
        spiBasicRead(dev, header, sizeof(header));

        status   = header[0];
        data_len = header[1];

        // Guard against buffer overflows caused by a corrupt length field.
        if (status != 0xFF && data_len > length) {
            status = 0xFF;
            fprintf(stderr,
                    "spiReadNoIrq: sensor data_len (%d) > buffer (%d), retrying...\n",
                    data_len, length);
        }
        elapsed_time = tssTimeDiff(start);
    }

    if(status == 0xFF) {
        gpiod_line_set_value(dev->cs_line, 1); // Set CS high
        fprintf(stderr, "spiReadNoIrq: timeout waiting for valid header\n");
        return TSS_ERR_TIMEOUT;
    }

    if (data_len > 0) {
        spiBasicRead(dev, out, data_len);
    }
    gpiod_line_set_value(dev->cs_line, 1); // Set CS high
    return data_len;
}

// -----------------------------------------------------------------------
// High-level read (uses dev->read_fn and dev->timeout)
// -----------------------------------------------------------------------

int spiRead(struct SpiDevice *dev, size_t num_bytes, uint8_t *out)
{
    if (dev->read_fn == NULL || num_bytes == 0) return 0;

    size_t total = 0;
    tss_time_t start = tssTimeGet();
    uint32_t elapsed_time = 0;
    while (total < num_bytes && elapsed_time <= dev->timeout) {
        size_t chunk = num_bytes - total;
        if (chunk > 255) chunk = 255;

        uint32_t remaining = dev->timeout - elapsed_time;
        int n = dev->read_fn(dev, out + total, (uint8_t)chunk, remaining);
        if (n < 0) return n;
        total += (size_t)n;
        elapsed_time = tssTimeDiff(start);
    }

    return (int)total;
}

// -----------------------------------------------------------------------
// Timeout accessors
// -----------------------------------------------------------------------

uint32_t spiGetTimeout(struct SpiDevice *dev)
{
    return dev->timeout;
}

void spiSetTimeout(struct SpiDevice *dev, uint32_t timeout_ms)
{
    dev->timeout = timeout_ms;
}

#endif /* __linux__ || unix */
