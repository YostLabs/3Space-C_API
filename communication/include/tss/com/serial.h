#ifndef __SERIAL_COM_CLASS_H__
#define __SERIAL_COM_CLASS_H__

#include "tss/com/com_class.h"
#include "tss/com/backend/serial/ser_device.h"
#include "tss/utility/ring_buf2.h"

struct SerialComClass {
    struct TSS_Com_Class com;
    struct SerialDevice port;

#if TSS_MINIMAL_SENSOR == 0
    //Required buffer for implementing peek functionality
    struct TSS_Ring_Buf2 in_ring;
    uint8_t _in_buffer[4096];
#endif

#if TSS_BUFFERED_WRITES
    uint8_t buffer[512];
    uint16_t index;
#endif
};

void create_serial_com_class(uint8_t port, struct SerialComClass *out);

int serial_com_auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *user_data);

#endif