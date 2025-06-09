#ifndef __TSS_COMMAND_H__
#define __TSS_COMMAND_H__

#include <stdint.h>

#define TSS_PARAM_IS_NULL(param) (param->count == 0)
#define TSS_PARAM_IS_STRING(param) (param->count > 0 && param->size == 0)

#define TSS_PARAM_NULL ((struct TSS_Param) { 0 })
#define TSS_PARAM(_count, _size) ((struct TSS_Param) { .count = (_count), .size = (_size) })

struct TSS_Param {
    uint8_t count;
    uint8_t size;   //Total size = count * size
                    //If size == 0, its a string and is null terminated.
};

struct TSS_Command {
    uint8_t num;

    const struct TSS_Param *in_format;
    const struct TSS_Param *out_format;
};

const struct TSS_Command* tssGetCommand(uint8_t num);

struct TSS_Setting {
    const char *name;

    const struct TSS_Param *in_format;
    const struct TSS_Param *out_format;
};

#define TSS_SETTING_RESPONSE_SIZE 3
struct TSS_Setting_Response {
    int8_t error;
    uint8_t num_success;
};

const struct TSS_Setting* tssGetSetting(const char *name);

#endif