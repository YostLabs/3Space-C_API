/**
 * @ Author: Andy Riedlinger
 * @ Create Time: 2025-6-20 11:45:00
 *
 * @ Description:
 * A wrapper class to add the peek functionality and
 * buffered writing to a minimal com class.
 * 
 * Also contains default implementations for some of the other
 * functions, such as read_until and clear that can be used
 * by minimal com classes.
 * 
 * @note:
 * If both TSS_MINIMAL_SENSOR=1 and TSS_BUFFERED_WRITES=0,
 * this class only wraps calls and provides no additional functionality
 */

#ifndef __TSS_RING_COM_H__
#define __TSS_RING_COM_H__

#include "tss/export.h"

#include "tss/com/com_class.h"
#include "tss/utility/ring_buf2.h"

#if __cplusplus
extern "C" {
#endif

struct TSS_Managed_Com_Class {
    struct TSS_Com_Class base;
    const struct TSS_Com_Class *child;

    //For peeking
    struct TSS_Ring_Buf2 read_ring;

    //For building writes before sending
    uint8_t *write_buffer;
    size_t write_buffer_size;
    uint16_t write_buffer_index;
};

TSS_API int tssCreateManagedCom(struct TSS_Com_Class *child, uint8_t *read_buf, size_t read_size, uint8_t *write_buf, size_t write_size, struct TSS_Managed_Com_Class *out);

//Applies the below default functions to the given com if that com does not already
//have an implementation for the function being set.
TSS_API void tssManagedComAddDefaults(struct TSS_Com_Class *com);


//Base functions that can be used for reading/clearing on any com class where user_data is a struct TSS_Com_Class and
//the read function and get/set timeout functions are implemented.
TSS_API int tssManagedComBaseReadUntil(uint8_t value, uint8_t *out, size_t size, void *com_class);
TSS_API void tssManagedComBaseClear(void *com_class);
TSS_API void tssManagedComBaseClearTimeout(void *com_class, uint32_t timeout_ms);

#if __cplusplus
}
#endif

#endif /* __TSS_RING_COM_H__ */