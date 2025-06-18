#ifndef __TSS_SERIAL_DEVICE_H__
#define __TSS_SERIAL_DEVICE_H__

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include "tss/com/backend/serial/win_serial.h"
#elif defined(__APPLE__) && defined(__MACH__)
#elif defined(__linux__) || defined(unix)
#endif

int serOpen(uint8_t port, uint32_t baudrate, struct SerialDevice *out);
void serClose(struct SerialDevice *ser);
int serConfigBufferSize(struct SerialDevice *ser, uint32_t in_size, uint32_t out_size);

uint32_t serWrite(struct SerialDevice *ser, const char *buffer, uint32_t len);
uint32_t serRead(struct SerialDevice *ser, char *buffer, uint32_t len);
uint32_t serReadImmediate(struct SerialDevice *ser, char *buffer, uint32_t len);
void serClear(struct SerialDevice *ser);

uint32_t serGetTimeout(struct SerialDevice *ser);
void serSetTimeout(struct SerialDevice *ser, uint32_t timeout_ms);

const char * serPortToName(uint8_t port, char *out, size_t size);

#define SER_ENUM_CONTINUE 0
#define SER_ENUM_STOP 1

//Enumerates all the ports and calls the supplied callback 1 time for each port.
//Return 1 of the above defines to control if enumeration stops or not
uint8_t serEnumeratePorts(uint8_t (*cb)(const char *name, uint8_t port, void *user_data), void *user_data);

#endif /* __TSS_SERIAL_DEVICE_H__ */