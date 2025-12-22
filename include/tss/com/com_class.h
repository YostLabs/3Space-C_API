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

struct TSS_Output_Stream {
    /**
     * @brief Writes the supplied data out.
     * @note If TSS_BUFFERED_WRITES=1, and the implementing object is not
     * being/intended to be wrapped by a TSS_Managed_Com_Class, this should instead save off
     * the supplied data in preparation of being sent out, rather then instantly sending.
     * @param com This com object.
     * @param bytes Data to send.
     * @param len Number of bytes located at \p bytes to send.
     * @return 0 on success, non-zero on error (len > available_space)
     */
    int (*write)(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

#if TSS_BUFFERED_WRITES
    /**
     * @brief Perform any initialization required when starting to send data. Such as resetting buffer indices.
     * @note Only required if TSS_BUFFERED_WRITES=1
     * @param com This com object.
     * @return 0 on success, non-zero on error
     */
    int (*begin_write)(struct TSS_Com_Class *com);

    /**
     * @brief Finalize a write by sending out any data written via write between begin_write and end_write
     * @note Only required if TSS_BUFFERED_WRITES=1
     * @param com This com object.
     * @return 0 on success, non-zero on error.
     */
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

/**
 * @brief Callback function for auto-detecting devices. When auto-detect is called, this 
 * function will be called for each detected device.
 * @param[in] com A detected communication object instance.
 * @param user_data User-specified data passed into the detection function.
 * @retval TSS_AUTO_DETECT_CONTINUE Continue detecting devices.
 * @retval TSS_AUTO_DETECT_STOP Stop detecting devices and return TSS_AUTO_DETECT_STOP.
 * @retval TSS_AUTO_DETECT_SUCCESS Stop detecting devices and return TSS_AUTO_DETECT_SUCCESS.
 * @warning \p com is the same address for each iteration of an auto detect function, so do not store it
 * if you are intentionally detecting multiple devices, as the pointer's data is overwritten each time the callback is called.
 */
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

    /**
     * @brief Opens/Initializes the communication object. If the communication 
     * object is already open, return success.
     * @return 0 on success, non-zero on error.
     */
    int (*open)(struct TSS_Com_Class *com);

    /**
     * @brief Closes/Deinitializes the communication object, freeing any used resources. 
     * If the communication object is already closed, return success
     * @return 0 on success, non-zero on error.
     */
    int (*close)(struct TSS_Com_Class *com);

    /**
     * @brief Rediscovers the communication object based on the callback and detect_data provided.
     * @note This function is only required to be implemented if reenumerates in \ref TSS_Com_Class is set to true.
     * @param[in,out] com The communication object to reenumerate.
     * @param cb A \ref TssComAutoDetectCallback function to be called for each detected device.
     * @param detect_data User data to be passed to the callback function. 
     * @retval TSS_AUTO_DETECT_SUCCESS Device found and stopping enumeration.
     * @retval TSS_AUTO_DETECT_STOP Stopped enumeration without finding a device.
     * @retval TSS_AUTO_DETECT_DONE Enumerated through all devices without \p cb returning TSS_AUTO_DETECT_SUCCESS or TSS_AUTO_DETECT_STOP.
     */
    int (*reenumerate)(struct TSS_Com_Class *com, TssComAutoDetectCallback cb, void *detect_data);

    /**
     * @brief Auto-detects devices and calls the provided callback for each detected device.
     * @note This function is not required to be implemented, it is for convenience.
     * @param[out] out Where to store the detected com class when the TssComAutoDetectCallback reports success.
     * @param cb A \ref TssComAutoDetectCallback function to be called for each detected device. If NULL, return TSS_AUTO_DETECT_SUCCESS
     * for the first detected device.
     * @param detect_data User data to be passed to the callback function. 
     * @retval TSS_AUTO_DETECT_SUCCESS Device found and stopping enumeration.
     * @retval TSS_AUTO_DETECT_STOP Stopped enumeration without finding a device.
     * @retval TSS_AUTO_DETECT_DONE Enumerated through all devices without \p cb returning TSS_AUTO_DETECT_SUCCESS or TSS_AUTO_DETECT_STOP.
     */
    int (*auto_detect)(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *detect_data);

    //WARNING: Internally, our API is designed to not dynamically allocate any memory. This means Yost Labs provided Com Classes
    //utilize the output location provided for auto_detect/com to store the detected devices. Because of this, it is not safe
    //to store/rely on the pointer passed to the callback function to not change.
    //This is an internal restriction adhered to by Yost Labs, but does not require the user to be aware of when implementing their own Com Classes.
    //The user should simply be aware that callback functions that work with their own custom com classes may not work with Yost Labs created com classes
    //if this restriction is not adhered to.

    //Note: reenumerate and auto_detect are functionally equivalent. The difference is only that case in which they are called.
    //By knowing the intention of reenumerating, it is possible the reenumeration function could be further optimized. But for most
    //cases, reenumerate can be implemented by simply calling auto_detect with the same parameters.
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

#endif /* __COM_CLASS_H__ */