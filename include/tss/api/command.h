#ifndef __TSS_COMMAND_H__
#define __TSS_COMMAND_H__

#include <stdint.h>
#include <stddef.h>
#include "tss/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TSS_PARAM_IS_NULL(param) ((param) == NULL || (param)->count == 0)
#define TSS_PARAM_IS_STRING(param) ((param)->count > 0 && (param)->size == 0)

//#define TSS_PARAM(_count, _size) ((struct TSS_Param) { .count = (_count), .size = (_size) })
#define TSS_PARAM_INITIALIZER(_count, _size) { .count = (_count), .size = (_size) }
#define TSS_PARAM(_count, _size) ((struct TSS_Param) TSS_PARAM_INITIALIZER(_count, _size))
#define TSS_PARAM_NULL_INITIALIZER TSS_PARAM_INITIALIZER(0, 0)
#define TSS_PARAM_NULL TSS_PARAM(0, 0)

struct TSS_Param {
    uint8_t count;
    uint16_t size;   //Total size = count * size
                    //If size == 0, its a string and is null terminated.
};

TSS_API void tssGetParamListSize(const struct TSS_Param *params, uint16_t *min_size, uint16_t *max_size);

struct TSS_Command {
    uint8_t num;

    const struct TSS_Param *in_format;
    const struct TSS_Param *out_format;
};

TSS_API const struct TSS_Command* tssGetCommand(uint8_t num);

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

TSS_API const struct TSS_Setting* tssGetSetting(const char *name);
TSS_API int tssSettingKeyCmp(const char* key, const char* key_format);

#ifdef __cplusplus
}
#endif

#endif