#include "tss_util.h"
#include "tss_string.h"
#include "tss_constants.h"

int tssUtilStreamSlotStringToCommands(const char *str, const struct TSS_Command* out[17])
{
    uint8_t num_slots_read = 0;
    struct TSS_Stream_Slot slot = {0};

    while(*str && num_slots_read < TSS_NUM_STREAM_SLOTS) {
        slot.cmd_num = strtol(str, (char**)&str, 10);
        if(*str == ':') {
            *str++;
            slot.param = strtol(str, (char**)&str, 10);
            slot.has_param = true;
        }

        //Reached the end
        if(slot.cmd_num == 255) {
            break;
        }
        
        num_slots_read++;

        //Add to the output
        out[num_slots_read-1] = tssGetCommand(slot.cmd_num);
        
        //Validate and advance string
        if(*str != ',') {
            return -1;
        }
        str++;
    }

    //Null terminate the command list
    printf("Num slots read: %d\n", num_slots_read);
    out[num_slots_read] = NULL;

    return 0;
}

void tssUtilCpy(void *_dst, const void *_src, size_t size)
{
    uint8_t *dst = (uint8_t*)_dst;
    const uint8_t *src = (uint8_t*)_src;
    for(size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

void tssUtilReverseCpy(void *_dst, const void *_src, size_t size)
{
    uint8_t *dst = (uint8_t*)_dst;
    const uint8_t *src = ((uint8_t*)_src) + size;
    for(size_t i = 0; i < size; i++) {
        dst[i] = src[size-1-i];
    }
}