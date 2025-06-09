#ifndef __TSS_UTIL_H__
#define __TSS_UTIL_H__

#include <stdint.h>
#include <stdbool.h>

#include "tss_command.h"

struct TSS_Stream_Slot {
    uint8_t cmd_num;
    uint8_t param;
    bool has_param;
};

int tssUtilStreamSlotStringToCommands(const char * str, const struct TSS_Command* out[17]);

// void tssUtilCpy(void *_dst, const void *_src, size_t size);
// void tssUtilReverseCpy(void *_dst, const void *_src, size_t size);


#endif /* __TSS_UTIL_H__ */
