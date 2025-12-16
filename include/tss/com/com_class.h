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

#include "tss/sys/config.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct TSS_Com_Class;

struct TSS_Input_Stream {
    //Read functions return the number of bytes read
    int (*read)(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out);
    //"Until" functions should include up to and including the value specified by value
    int(*read_until)(struct TSS_Com_Class *com, uint8_t value, uint8_t *out, size_t size);

    //Peek functions return the number of bytes read.

    //If not enough internal buffer space to peek the requested amount,
    //return TSS_ERR_INSUFFICIENT_BUFFER.

    //Peek should have a minimum look ahead length of 50. In general, if the available
    //look ahead length is not long enough, accidentally validating corrupt data becomes more likely.
    //For peek sizes, we recommend either 64, 256, 1024, or >=4096.

    //The general rule is you want the max peek size to be greater than the response length of
    //the largest command you will be receving a response to.

    //32 is the absolute lowest we would ever suggest, in general, 256 is the recommended
    //budget size, 1024 for if performing file streaming, and 4096 guarantees the largest
    //possible command response can fit in the buffer (The File IO ReadBytes command where you 
    //can specify number of bytes to read)

    //If peek functionality is not desired at all, TSS_MINIMAL_SENSOR can be set in the tss/sys/config.h
    //and a version of the API that does NOT validate by looking ahead is available. Do note that version
    //has no capability of automatically realigning/recovering from corrupt data.
#if !(TSS_MINIMAL_SENSOR)
    int (*peek)(struct TSS_Com_Class *com, size_t start, size_t num_bytes, uint8_t *out);
    int(*peek_until)(struct TSS_Com_Class *com, size_t start, uint8_t value, uint8_t *out, size_t size);
    size_t (*peek_capacity)(struct TSS_Com_Class *com);
    size_t (*length)(struct TSS_Com_Class *com);
#endif

    //Note: Setting timeout to 0 should result in an instantaneous read, not an indefinite block
    void (*set_timeout)(struct TSS_Com_Class *com, uint32_t timeout);
    uint32_t (*get_timeout)(struct TSS_Com_Class *com);

    void (*clear_immediate)(struct TSS_Com_Class *com);
    void (*clear_timeout)(struct TSS_Com_Class *com, uint32_t timeout);
};


//Modify me to have a write and write_send. Allow the OutputStream to buffer the write
//until it needs to be sent if desired. (Will help for things like I2C and SPI)
//Probably just have a write_begin, write, and write_end.
//Have compiler flags to not include.
struct TSS_Output_Stream {
    int (*write)(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

#if TSS_BUFFERED_WRITES
    int (*begin_write)(struct TSS_Com_Class *com);
    int (*end_write)(struct TSS_Com_Class *com);
#endif
};

//Helpers for conditionally compiling in the begin/end writes
#if TSS_BUFFERED_WRITES
#define TSS_COM_BEGIN_WRITE(com) ((com)->api->out.begin_write((com)))
#define TSS_COM_END_WRITE(com) ((com)->api->out.end_write((com)))
#else
#define TSS_COM_BEGIN_WRITE(com)
#define TSS_COM_END_WRITE(com)
#endif

typedef int (*TssComAutoDetectCallback)(struct TSS_Com_Class *com, void *user_data);

//The callback should return one of CONTINUE, STOP, or SUCCESS
//The call to auto_detect will return STOP, SUCCESS, or DONE based on the return value
//of the callback, with DONE being added for if there are no more devices and the callback
//returned CONTINUE

//Continue detecting devices. If no more, returns TSS_AUTO_DETECT_DONE
#define TSS_AUTO_DETECT_CONTINUE 0
//Stop detecting devices and return TSS_AUTO_DETECT_STOP
#define TSS_AUTO_DETECT_STOP 1
//Stop detecting devices and return TSS_AUTO_DETECT_SUCCESS
#define TSS_AUTO_DETECT_SUCCESS 2
//No more devices to detect
#define TSS_AUTO_DETECT_DONE 3

struct TSS_Com_Class_API {
    struct TSS_Input_Stream in;
    struct TSS_Output_Stream out;

    //Open the device. If the device is already open
    //leave the device open and return success
    int (*open)(struct TSS_Com_Class *com);

    //Close the device. If the device is already closed
    //return success.
    int (*close)(struct TSS_Com_Class *com);
    //Some com devices may need to be rediscovered when reconnecting.
    //This function provides a way to reconnect to a device, while using the same
    //com object, based on a condition provided by a callback and its user data.
    //(It is common for a USB connection to reenumerate when 
    //the sensor restarts/enters bootloader)
    int (*reenumerate)(struct TSS_Com_Class *com, TssComAutoDetectCallback cb, void *detect_data);

    //Only required to be implemented if reenumerates is set to true.
    //Alternatively, leave reenumerate as false and manually handle connection
    //state in your application when changes are made.
    //NOTE: Detect data is passed to the TssComAutoDetectCallback as the detect_data parameter
    //If cb is NULL, the function should not call it and treat its return value as TSS_AUTO_DETECT_SUCCESS
    //*out is the storage location used for the detected devices that are passed to TssComAutoDetectCallback
    int (*auto_detect)(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *detect_data);
};


struct TSS_Com_Class {
    const struct TSS_Com_Class_API *api;
    bool reenumerates;
};

// Inline wrapper functions for convenient API access
static inline int tss_com_open(struct TSS_Com_Class *com)
{
    return com->api->open(com);
}

static inline int tss_com_close(struct TSS_Com_Class *com)
{
    return com->api->close(com);
}

static inline int tss_com_reenumerate(struct TSS_Com_Class *com, TssComAutoDetectCallback cb, void *detect_data)
{
    return com->api->reenumerate(com, cb, detect_data);
}

static inline int tss_com_auto_detect(struct TSS_Com_Class_API *api, struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *detect_data)
{
    return api->auto_detect(out, cb, detect_data);
}

// Input Stream wrappers
static inline int tss_com_read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out)
{
    return com->api->in.read(com, num_bytes, out);
}

static inline int tss_com_read_until(struct TSS_Com_Class *com, uint8_t value, uint8_t *out, size_t size)
{
    return com->api->in.read_until(com, value, out, size);
}

#if !(TSS_MINIMAL_SENSOR)
static inline int tss_com_peek(struct TSS_Com_Class *com, size_t start, size_t num_bytes, uint8_t *out)
{
    return com->api->in.peek(com, start, num_bytes, out);
}

static inline int tss_com_peek_until(struct TSS_Com_Class *com, size_t start, uint8_t value, uint8_t *out, size_t size)
{
    return com->api->in.peek_until(com, start, value, out, size);
}

static inline size_t tss_com_peek_capacity(struct TSS_Com_Class *com)
{
    return com->api->in.peek_capacity(com);
}

static inline size_t tss_com_length(struct TSS_Com_Class *com)
{
    return com->api->in.length(com);
}
#endif

static inline void tss_com_set_timeout(struct TSS_Com_Class *com, uint32_t timeout)
{
    com->api->in.set_timeout(com, timeout);
}

static inline uint32_t tss_com_get_timeout(struct TSS_Com_Class *com)
{
    return com->api->in.get_timeout(com);
}

static inline void tss_com_clear_immediate(struct TSS_Com_Class *com)
{
    com->api->in.clear_immediate(com);
}

static inline void tss_com_clear_timeout(struct TSS_Com_Class *com, uint32_t timeout)
{
    com->api->in.clear_timeout(com, timeout);
}

// Output Stream wrappers
static inline int tss_com_write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len)
{
    return com->api->out.write(com, bytes, len);
}

#if TSS_BUFFERED_WRITES
static inline int tss_com_begin_write(struct TSS_Com_Class *com)
{
    return com->api->out.begin_write(com);
}

static inline int tss_com_end_write(struct TSS_Com_Class *com)
{
    return com->api->out.end_write(com);
}
#endif

#endif /* __COM_CLASS_H__ */