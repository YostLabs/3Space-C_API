/**
 * @ Author: Andy Riedlinger
 * @ Create Time: 2025-5-22 1:01:00
 *
 * @ Description:
 * 
 * Required functions for sensor communication
 */

#ifndef __COM_CLASS_H__
#define __COM_CLASS_H__

#include "tss_config.h"
#include <stdint.h>

struct TSS_Input_Stream {
    //Returns number of bytes read
    int (*read)(size_t num_bytes, uint8_t *out, void *user_data);
    int (*peek)(size_t num_bytes, uint8_t *out, void *user_data);

    //Separated to allow the input streams to implement potentially more efficient
    //iterative reading logic then the API calling read/peek multiple times
    int(*read_until)(uint8_t value, uint8_t *out, size_t size, void *user_data);
    int(*peek_until)(uint8_t value, uint8_t *out, size_t size, void *user_data);

    size_t (*length)(void *user_data);
    void (*set_timeout)(uint32_t timeout, void *user_data);

    void (*clear_immediate)(void *user_data);
    void (*clear_timeout)(void *user_data, uint32_t timeout);
};



//Modify me to have a write and write_send. Allow the OutputStream to buffer the write
//until it needs to be sent if desired. (Will help for things like I2C and SPI)
//Probably just have a write_begin, write, and write_end.
//Have compiler flags to not include.
struct TSS_Output_Stream {
    int (*write)(const uint8_t *bytes, uint32_t len, void *user_data);

#if TSS_BUFFERED_WRITES
    int (*begin_write)(void *user_data);
    int (*end_write)(void *user_data);
#endif
};

struct TSS_Com_Class {
    struct TSS_Input_Stream in;
    struct TSS_Output_Stream out;

    int (*open)(void *user_data);
    int (*close)(void *user_data);

    void *user_data;
};

#endif /* __COM_CLASS_H__ */