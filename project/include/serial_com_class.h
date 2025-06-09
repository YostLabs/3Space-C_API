#ifndef __SERIAL_COM_CLASS_H__
#define __SERIAL_COM_CLASS_H__

#include "tss_com_class.h"
#include "windows_serial.h"

struct SerialComClass {
    struct TSS_Com_Class com;

    struct SerialDevice port;

#if TSS_BUFFERED_WRITES
    uint8_t buffer[512];
    uint16_t index;
#endif
};

void create_serial_com_class(uint8_t port, struct SerialComClass *out);

#endif