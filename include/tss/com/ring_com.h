/**
 * @ Author: Andy Riedlinger
 * @ Create Time: 2025-6-20 11:45:00
 *
 * @ Description:
 * A wrapper class to add the peek functionality and
 * buffered writing to a minimal com class.
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

struct TSS_Ring_Com_Class {
    struct TSS_Com_Class base;
    const struct TSS_Com_Class *child;

    //For peeking
    struct TSS_Ring_Buf2 read_ring;

    //For building writes before sending
    uint8_t *write_buffer;
    size_t write_buffer_size;
    uint16_t write_buffer_index;
};

TSS_API int tssCreateRingCom(const struct TSS_Com_Class *child, uint8_t *read_buf, size_t read_size, uint8_t *write_buf, size_t write_size, struct TSS_Ring_Com_Class *out);

#if __cplusplus
}
#endif

#endif /* __TSS_RING_COM_H__ */