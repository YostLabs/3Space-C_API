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

struct TSS_Input_Stream {
    //Read functions return the number of bytes read
    int (*read)(size_t num_bytes, uint8_t *out, void *user_data);
    //"Until" functions should include up to and including the value specified by value
    int(*read_until)(uint8_t value, uint8_t *out, size_t size, void *user_data);

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
    int (*peek)(size_t start, size_t num_bytes, uint8_t *out, void *user_data);
    int(*peek_until)(size_t start, uint8_t value, uint8_t *out, size_t size, void *user_data);
    size_t (*peek_capacity)(void *user_data);
    size_t (*length)(void *user_data);
#endif

    //Note: Setting timeout to 0 should result in an instantaneous read, not an indefinite block
    void (*set_timeout)(uint32_t timeout, void *user_data);
    uint32_t (*get_timeout)(void *user_data);

    void (*clear_immediate)(void *user_data);
    void (*clear_timeout)(void *user_data, uint32_t timeout);
};


//Modify me to have a write and write_send. Allow the OutputStream to buffer the write
//until it needs to be sent if desired. (Will help for things like I2C and SPI)
//Probably just have a write_begin, write, and write_end.
//Have compiler flags to not include.
struct TSS_Output_Stream {
    int (*write)(const uint8_t *bytes, size_t len, void *user_data);

#if TSS_BUFFERED_WRITES
    int (*begin_write)(void *user_data);
    int (*end_write)(void *user_data);
#endif
};

//Helpers for conditionally compiling in the begin/end writes
#if TSS_BUFFERED_WRITES
#define TSS_COM_BEGIN_WRITE(com) ((com)->out.begin_write((com->user_data)))
#define TSS_COM_END_WRITE(com) ((com)->out.end_write((com->user_data)))
#else
#define TSS_COM_BEGIN_WRITE(com)
#define TSS_COM_END_WRITE(com)
#endif

struct TSS_Com_Class;
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


//NOTE: When creating a custom Com Class with this struct included, this must be the first element
//of that struct. You should be able to freely cast between a (struct TSS_Com_Class*) and (struct MyCustomComClass*).
struct TSS_Com_Class {
    struct TSS_Input_Stream in;
    struct TSS_Output_Stream out;

    //Open the device. If the device is already open
    //leave the device open and return success
    int (*open)(void *user_data);

    //Close the device. If the device is already closed
    //return success.
    int (*close)(void *user_data);

    //Some com devices may need to be rediscovered when reconnecting.
    //This function provides a way to reconnect to a device, while using the same
    //com object, based on a condition provided by a callback and its user data.
    //(It is common for a USB connection to reenumerate when 
    //the sensor restarts/enters bootloader)
    int (*reenumerate)(TssComAutoDetectCallback cb, void *detect_data, void *user_data);

    //Only required to be implemented if reenumerates is set to true.
    //Alternatively, leave reenumerate as false and manually handle connection
    //state in your application when changes are made.
    //NOTE: This user data is user data meant for the callback function provided, not
    //the user data of a specific instance of a ComClass.
    //If cb is NULL, the function should not call it and treat its return value as TSS_AUTO_DETECT_SUCCESS
    int (*auto_detect)(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *detect_data);


    //Common info passed to most ComClass functions.
    //Typically this will be a pointer to a parent of
    //this struct to includes information required for
    //that specific ComClass operation.
    //When calling com class functions that take user
    //data, make sure to pass this as well.
    void *user_data;
};

static int tssComDefaultReadUntil(uint8_t value, uint8_t *out, size_t size, void *user_data);
static void tssComDefaultClearImmediate(void *user_data);
static void tssComDefaultClearTimeout(void *user_data, uint32_t timeout_ms);

#endif /* __COM_CLASS_H__ */