#ifndef __SERIAL_COM_CLASS_H__
#define __SERIAL_COM_CLASS_H__

#include "tss/com/managed_com.h"
#include "tss/com/backend/serial/ser_device.h"
#include "tss/utility/ring_buf2.h"

struct SerialComClass {
    struct TSS_Managed_Com_Class base;
    struct TSS_Com_Class serial_com;
    struct SerialDevice port;

#if TSS_MINIMAL_SENSOR == 0
    uint8_t read_buffer[4096];
#endif

#if TSS_BUFFERED_WRITES
    uint8_t write_buffer[512];
#endif
};

void create_serial_com_class(uint8_t port, struct SerialComClass *out);

int serial_com_auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *user_data);

#endif