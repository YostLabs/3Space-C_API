#if defined(__linux__) || defined(unix)

#include "tss/com/backend/serial/ser_device.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <dirent.h>
#include <stdlib.h>
#include <poll.h>

static speed_t to_baud(uint32_t baudrate)
{
    switch(baudrate) {
        case 50:      return B50;
        case 75:      return B75;
        case 110:     return B110;
        case 134:     return B134;
        case 150:     return B150;
        case 200:     return B200;
        case 300:     return B300;
        case 600:     return B600;
        case 1200:    return B1200;
        case 1800:    return B1800;
        case 2400:    return B2400;
        case 4800:    return B4800;
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
        case 500000:  return B500000;
        case 576000:  return B576000;
        case 921600:  return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        default:      return B0;
    }
}

int serOpen(uint8_t port, uint32_t baudrate, struct SerialDevice *out)
{
    #define DEV_MAXNAME 32
    char name[DEV_MAXNAME];

    *out = (struct SerialDevice) {
        .fd = -1,
        .port = port,
        .timeout = 1000,
        .blocking = true
    };

    serPortToName(port, name, DEV_MAXNAME);

    int fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(fd < 0) {
        printf("Failed to open serial port %s: %s\n", name, strerror(errno));
        return -1;
    }
    out->fd = fd;

    struct termios tty;
    if(tcgetattr(fd, &tty) != 0) {
        close(fd);
        out->fd = -1;
        return -1;
    }

    speed_t baud = to_baud(baudrate);
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // 8N1, raw mode
    tty.c_cflag &= ~(uint32_t)PARENB;           // No parity
    tty.c_cflag &= ~(uint32_t)CSTOPB;           // 1 stop bit
    tty.c_cflag &= ~(uint32_t)CSIZE;
    tty.c_cflag |= CS8;                          // 8 data bits
#ifdef CRTSCTS
    tty.c_cflag &= ~(uint32_t)CRTSCTS;          // No hardware flow control
#endif
    tty.c_cflag |= CREAD | CLOCAL;              // Enable receiver, ignore modem lines

    tty.c_lflag &= ~(uint32_t)(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    tty.c_iflag &= ~(uint32_t)(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(uint32_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(uint32_t)(OPOST | ONLCR);

    // poll() handles timeouts; set termios to return immediately on read()
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if(tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        out->fd = -1;
        return -1;
    }

    return 0;
}

void serClose(struct SerialDevice *ser)
{
    if(ser->fd < 0) return;
    close(ser->fd);
    ser->fd = -1;
}

int serConfigBufferSize(struct SerialDevice *ser, uint32_t in_size, uint32_t out_size)
{
    // Linux manages kernel serial buffers internally; nothing to configure here.
    (void)ser; (void)in_size; (void)out_size;
    return 0;
}

uint32_t serRead(struct SerialDevice *ser, char *buffer, uint32_t len)
{
    if(len == 0 || ser->fd < 0) return 0;

    struct pollfd pfd = { .fd = ser->fd, .events = POLLIN };
    int timeout_ms = ser->blocking ? (int)ser->timeout : 0;
    int ret = poll(&pfd, 1, timeout_ms);
    if(ret <= 0) return 0;

    ssize_t n = read(ser->fd, buffer, len);
    if(n < 0) return 0;
    return (uint32_t)n;
}

uint32_t serWrite(struct SerialDevice *ser, const char *buffer, uint32_t len)
{
    if(len == 0 || ser->fd < 0) return 0;
    ssize_t n = write(ser->fd, buffer, len);
    if(n < 0) return 0;
    return (uint32_t)n;
}

void serClear(struct SerialDevice *ser)
{
    if(ser->fd < 0) return;
    tcflush(ser->fd, TCIFLUSH);
}

uint32_t serGetTimeout(struct SerialDevice *ser)
{
    if(!ser->blocking) return 0;
    return ser->timeout;
}

void serSetTimeout(struct SerialDevice *ser, uint32_t timeout_ms)
{
    if(timeout_ms == 0) {
        ser->blocking = false;
        return;
    }
    ser->blocking = true;
    ser->timeout = timeout_ms;
}

// Port encoding:
//   port 0-127   -> /dev/ttyUSB<port>
//   port 128-255 -> /dev/ttyACM<port - 128>
const char * serPortToName(uint8_t port, char *out, size_t size)
{
    if(port < 128) {
        snprintf(out, size, "/dev/ttyUSB%u", (unsigned)port);
    } else {
        snprintf(out, size, "/dev/ttyACM%u", (unsigned)(port - 128u));
    }
    return out;
}

uint8_t serEnumeratePorts(uint8_t (*cb)(const char *name, uint8_t port, void *user_data), void *user_data)
{
    static const struct {
        const char *prefix;
        uint8_t offset;
    } device_types[] = {
        { "ttyUSB", 0   },   // port = suffix + 0   (range 0-127)
        { "ttyACM", 128 },   // port = suffix + 128  (range 128-255)
    };
    static const size_t num_types = sizeof(device_types) / sizeof(device_types[0]);

    uint8_t ports_discovered = 0;

    DIR *dev_dir = opendir("/dev");
    if(!dev_dir) return 0;

    struct dirent *entry;
    while((entry = readdir(dev_dir)) != NULL) {
        for(size_t i = 0; i < num_types; i++) {
            const char *prefix = device_types[i].prefix;
            size_t prefix_len = strlen(prefix);

            if(strncmp(entry->d_name, prefix, prefix_len) != 0) continue;

            const char *num_str = entry->d_name + prefix_len;
            char *end;
            long num = strtol(num_str, &end, 10);
            if(end == num_str || *end != '\0' || num < 0) break;

            unsigned long encoded = (unsigned long)num + device_types[i].offset;
            if(encoded > 255) break;

            char full_path[64];
            snprintf(full_path, sizeof(full_path), "/dev/%s", entry->d_name);

            ports_discovered++;
            if(cb(full_path, (uint8_t)encoded, user_data) == SER_ENUM_STOP) {
                closedir(dev_dir);
                return ports_discovered;
            }
            break;
        }
    }

    closedir(dev_dir);
    return ports_discovered;
}

#endif /* __linux__ || unix */
