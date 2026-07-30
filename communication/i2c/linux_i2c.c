#if defined(__linux__) || defined(unix)

#include "tss/com/backend/i2c/i2c_device.h"
#include "tss/constants.h"
#include "tss/sys/time.h"
#include "tss/errors.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define IRQ_ACTIVE_STATE 0
#define IRQ_INACTIVE_STATE 1

static int i2cReadNoIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);
static int i2cReadWithDataAvailableIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);
static int i2cReadWithFullIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms);

// -----------------------------------------------------------------------
// Open / Close
// -----------------------------------------------------------------------

int i2cOpen(I2cPortId id, uint32_t speed_hz, struct I2cDevice *out)
{
    //The user is not required to provide these numbers. They default to 0, even though -1 means not used.
    //So if both are 0, will set to -1 to indicate not used. If one is 0 and the other is not, will print a warning to the user.
    //The preferred way of setting these is calling configurePinMode.
    if(id.data_available_line_num == 0 && id.data_loaded_line_num == 0) {
        //If both lines are set to 0, then we will not use IRQs for reading.
        id.data_available_line_num = -1;
        id.data_loaded_line_num = -1;
    }
    else if(id.data_available_line_num == 0 || id.data_loaded_line_num == 0) {
        fprintf(stderr, "Warning: One IRQ line number is set to 0, while the other is not. Was this intentional?\n");
    }

    *out = (struct I2cDevice) {
        .id = id,
        .fd            = -1,
        .speed_hz      = speed_hz,
        .timeout       = 1000,
        .header_timeout= 1,
        .read_fn       = i2cReadNoIrq,
    };

    //----------------Configure GPIO----------------
    i2cConfigurePinMode(out, id.data_available_line_num, id.data_loaded_line_num);

    //----------------Configure I2C device----------------

    // 1. Open the I2C bus file descriptor
    int fd = open(id.device_name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open I2C device %s: %s\n", id.device_name, strerror(errno));
        i2cClose(out);
        return -1;
    }

    // 2. Set the target I2C device address
    if (ioctl(fd, I2C_SLAVE, id.bus_address) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        close(fd);
        return -1;
    }

    // 3. Configure the timeout
    // This timeout is separate from the read/write logical timeout
    // This timeout is how long the kernel will wait for a response before
    // giving up a transaction, whereas the stored timeout is the maximum time
    // the API will reattempt transactions before giving up. It is desired for
    // the individual transaction timeout to be fast, because the device should be
    // ACK/NACK to fail instantly if something goes wrong. If the device isn't responding
    // it should fail ASAP and try again.
    if(ioctl(fd, I2C_TIMEOUT, 100 / 10) < 0) {
        perror("i2cOpen: ioctl I2C_TIMEOUT failed");
        close(fd);
        return -1;
    }

    out->fd = fd;

    return 0;
}

int i2cConfigurePinMode(struct I2cDevice *dev, int data_available_line_num, int data_loaded_line_num)
{
    //Cleanup old lines if any
    if(dev->data_available_line != NULL && dev->id.data_available_line_num != data_available_line_num) {
        gpiod_line_release(dev->data_available_line);
        dev->data_available_line = NULL;
    }

    if(dev->data_loaded_line != NULL && dev->id.data_loaded_line_num != data_loaded_line_num) {
        gpiod_line_release(dev->data_loaded_line);
        dev->data_loaded_line = NULL;
    }

    dev->id.data_available_line_num = data_available_line_num;
    dev->id.data_loaded_line_num = data_loaded_line_num;

    if(data_available_line_num == -1 && data_loaded_line_num == -1) {
        if(dev->chip != NULL) {
            gpiod_chip_close(dev->chip);
            dev->chip = NULL;
        }
        dev->read_fn = i2cReadNoIrq;
        return 0;
    }

    //Initialise GPIO chip if not already done
    if(dev->chip == NULL) {
        dev->chip = gpiod_chip_open(dev->id.chip_path);
        if(!dev->chip) {
            perror("gpiod_chip_open");
            return -1;
        }   
    }

    //Configure the new lines
    if(data_available_line_num >= 0) {
        dev->data_available_line = gpiod_chip_get_line(dev->chip, (unsigned int)data_available_line_num);
        if(dev->data_available_line != NULL) {
            if(gpiod_line_request_input(dev->data_available_line, "i2c_data_available") < 0) {
                perror("gpiod_line_request_input");
                return -1;
            }
        }
    }
    if(data_loaded_line_num >= 0) {
        dev->data_loaded_line = gpiod_chip_get_line(dev->chip, (unsigned int)data_loaded_line_num);
        if(dev->data_loaded_line != NULL) {
            if(gpiod_line_request_input(dev->data_loaded_line, "i2c_data_loaded") < 0) {
                perror("gpiod_line_request_input");
                return -1;
            }
        }
    }

    if(dev->data_available_line != NULL) {
        if(dev->data_loaded_line != NULL) {
            dev->read_fn = i2cReadWithFullIrq;
        }
        else {
            dev->read_fn = i2cReadWithDataAvailableIrq;
        }
    }
    return 0;
}

void i2cClose(struct I2cDevice *dev)
{
    if (dev->fd < 0) return;

    if(dev->data_available_line != NULL) {
        gpiod_line_release(dev->data_available_line);
        dev->data_available_line = NULL;
    }
    if(dev->data_loaded_line != NULL) {
        gpiod_line_release(dev->data_loaded_line);
        dev->data_loaded_line = NULL;
    }
    if(dev->chip != NULL) {
        gpiod_chip_close(dev->chip);
        dev->chip = NULL;
    }
    
    close(dev->fd);
    dev->fd = -1;
}

// -----------------------------------------------------------------------
// Write
// -----------------------------------------------------------------------

int i2cWrite(struct I2cDevice *dev, const uint8_t *data, size_t len)
{
    if (len == 0 || dev->fd < 0) return 0;
    unsigned char write_buf[2];
    write_buf[0] = TSS_TRANSACTION_WRITE_DATA_BYTE;

    size_t write_index = 0;

    while (len > 0) {
        uint8_t send_len = (len > 255) ? 255 : (uint8_t)len;
        write_buf[1] = send_len;

        // Define 2 messages to send in a single transaction
        struct i2c_msg msgs[2];

        // First message: The 2-byte header (generates START condition)
        msgs[0].addr  = dev->id.bus_address;
        msgs[0].flags = 0;          // 0 = Write operation
        msgs[0].len   = 2;
        msgs[0].buf   = write_buf;

        // Second message: The payload (NO START condition, continues the frame)
        msgs[1].addr  = dev->id.bus_address;
        msgs[1].flags = I2C_M_NOSTART; // Continue payload without sending START/address again
        msgs[1].len   = send_len;
        msgs[1].buf   = (uint8_t *)(data + write_index);

        // Package both messages into the ioctl payload
        struct i2c_rdwr_ioctl_data rdwr_data;
        rdwr_data.msgs  = msgs;
        rdwr_data.nmsgs = 2; // Execute both back-to-back in 1 bus transaction

        if (ioctl(dev->fd, I2C_RDWR, &rdwr_data) < 0) {
            perror("I2C_RDWR write failed");
            return -1; // Return error code
        }

        len -= send_len;
        write_index += send_len;
    }

    return 0;
}

// -----------------------------------------------------------------------
// Protocol read (no-IRQ polling style)
// -----------------------------------------------------------------------

int i2cReadNoIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms)
{
    (void) timeout_ms;
    
    if (length == 0) return 0;

    // Send READ_DATA_WITH_SIZE command followed by the requested byte count.
    uint8_t header[2] = { TSS_TRANSACTION_READ_DATA_WITH_SIZE_BYTE, length };
    tss_time_t start = tssTimeGet();
    uint32_t elapsed = 0;
    ssize_t write_result = -1;
    while(elapsed <= timeout_ms && write_result < 0) {
        write_result = write(dev->fd, header, sizeof(header));
        elapsed = tssTimeDiff(start);
    }
    if(write_result < 0) {
        return -1;
    }


    uint8_t status = 0xFF, data_len = 0;
    start = tssTimeGet();
    uint32_t elapsed_time = 0;
    while (status == 0xFF && elapsed_time <= dev->header_timeout) {
        memset(header, 0xFF, sizeof(header));
        ssize_t num_read = read(dev->fd, header, sizeof(header));
        if(num_read < 0) {
            if(errno == ETIMEDOUT) {
                return TSS_ERR_TIMEOUT;
            }
            else {
                return -1;
            }
        }

        status   = header[0];
        data_len = header[1];

        // Guard against buffer overflows caused by a corrupt length field.
        if (status != 0xFF && data_len > length) {
            status = 0xFF;
            fprintf(stderr,
                    "i2cReadNoIrq: sensor data_len (%d) > buffer (%d), retrying...\n",
                    data_len, length);
        }
        elapsed_time = tssTimeDiff(start);
    }

    if(status == 0xFF) {
        fprintf(stderr, "i2cReadNoIrq: timeout waiting for valid header\n");
        return TSS_ERR_TIMEOUT;
    }

    if (data_len > 0) {
        if(data_len > length) {
            fprintf(stderr, "i2cReadNoIrq: Unexpected data_len (%d) > buffer (%d)\n", data_len, length);
            return -1;
        }
        ssize_t num_read = read(dev->fd, out, data_len);
        if(num_read < 0) {
            return -1;
        }
    }
    return data_len;
}

static int i2cReadWithDataAvailableIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms)
{
    if (length == 0) return 0;

    // Wait for the Data Available line to go low
    tss_time_t start = tssTimeGet();
    uint32_t elapsed_time = 0;
    while (gpiod_line_get_value(dev->data_available_line) == IRQ_INACTIVE_STATE) {
        if(elapsed_time > timeout_ms) {
            return TSS_ERR_TIMEOUT;
        }
        elapsed_time = tssTimeDiff(start);
    }

    //Then do a normal read
    return i2cReadNoIrq(dev, out, length, timeout_ms);
}

static int i2cReadWithFullIrq(struct I2cDevice *dev, uint8_t *out, uint8_t length, uint32_t timeout_ms)
{
    if(length == 0) return 0;
    //Wait for data_loaded pin to reset
    //This should normally take no time, but it is possible to read so fast
    //back to back that his pin may not have been deasserted yet.
    //Doing this check here instead of after reading to avoid wasting time when could continue processing.
    tss_time_t start_time = tssTimeGet();
    tss_time_t elapsed_time = 0;

    while(gpiod_line_get_value(dev->data_loaded_line) == IRQ_ACTIVE_STATE) {
        if(elapsed_time > timeout_ms + dev->header_timeout) {
            //There might actually be data loaded that shouldn't be there if this times out.
            //Clear it.

            //Have to clear using the no irq mode otherwise the same issue will occur.
            uint8_t clear_buffer[40];
            int len;
            do {
                len = i2cReadNoIrq(dev, clear_buffer, sizeof(clear_buffer), 0);
            } while(len > 0);
            return TSS_ERR_TIMEOUT;
        }
        elapsed_time = tssTimeDiff(start_time);
    }

    //Wait for data to be available
    elapsed_time = 0;
    while(gpiod_line_get_value(dev->data_available_line) == IRQ_INACTIVE_STATE) {
        if(elapsed_time > timeout_ms) {
            return TSS_ERR_TIMEOUT;
        }
        elapsed_time = tssTimeDiff(start_time);
    }

    //Start the read
    uint8_t header[2] = { TSS_TRANSACTION_READ_DATA_WITH_SIZE_BYTE, length };
    ssize_t num_written = write(dev->fd, header, sizeof(header));
    if(num_written < 0) {
        return -1;
    }

    //Wait until the data is loaded
    start_time = tssTimeGet();
    elapsed_time = 0;
    while(gpiod_line_get_value(dev->data_loaded_line) == IRQ_INACTIVE_STATE) {
        if(elapsed_time > timeout_ms + dev->header_timeout) {
            return -1; //Somehow failed to load data
        }
        elapsed_time = tssTimeDiff(start_time);
    }

    //Read the header
    ssize_t num_read = read(dev->fd, header, sizeof(header));
    if(num_read < 0) {
        return -1;
    }
    uint8_t status = header[0];
    uint8_t data_len = header[1];

    //This should never occur when using the data loaded pin, but checking anyways
    if(status == 0xFF) {
        fprintf(stderr, "i2cReadWithFullIrq: Unexpected 0xFF status when using full data IRQ\n");
        return -1;
    }

    if(data_len > 0) {
        //This shouldn't occur unless there is an
        //issue with the I2C lines. Checking anyways
        //to ensure no buffer overruns.
        if(data_len > length) {
            fprintf(stderr, "i2cReadWithFullIrq: Unexpected data_len (%d) > buffer (%d)\n", data_len, length);
            return -1;
        }
        num_read = read(dev->fd, out, data_len);
        if(num_read < 0) {
            return -1;
        }
    }

    return data_len;
}

// -----------------------------------------------------------------------
// High-level read (uses dev->read_fn and dev->timeout)
// -----------------------------------------------------------------------

int i2cRead(struct I2cDevice *dev, size_t num_bytes, uint8_t *out)
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
        if(n >= 0) {
            total += (size_t)n;
        }
        else if(n != TSS_ERR_TIMEOUT) {
            //Hardware error. Timeout errors
            //do not propagate, just return less
            //data than requested. Other errors are fatal.
            return n;
        }
        
        elapsed_time = tssTimeDiff(start);
    }

    return (int)total;
}

// -----------------------------------------------------------------------
// Timeout accessors
// -----------------------------------------------------------------------

uint32_t i2cGetTimeout(const struct I2cDevice *dev)
{
    return dev->timeout;
}

void i2cSetTimeout(struct I2cDevice *dev, uint32_t timeout_ms)
{
    dev->timeout = timeout_ms;
}

#endif /* __linux__ || unix */
