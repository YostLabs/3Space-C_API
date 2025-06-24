/**
 * @ Author: Andy Riedlinger
 * @ Create Time: 2025-5-22 1:01:00
 *
 * @ Description:
 */

#ifndef __TSS_API_H__
#define __TSS_API_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "tss/export.h"

#include "tss/com/com_class.h"
#include "tss/api/command.h"
#include "tss/api/header.h"

#ifdef __cplusplus
extern "C" {
#endif

TSS_API int tssWriteCommand(const struct TSS_Com_Class *com, bool header, const struct TSS_Command *command, const void **data);

//Returns -Error or >= 0 checksum
TSS_API int tssReadCommand(const struct TSS_Com_Class *com, const struct TSS_Command *command, ...);
TSS_API int tssReadCommandV(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list args);
TSS_API int tssReadCommandVp(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list *args);
TSS_API int tssReadCommandChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Command *command);


TSS_API int tssReadHeader(const struct TSS_Com_Class *com, const struct TSS_Header_Info *header_info, struct TSS_Header *out);
TSS_API int tssPeekHeader(const struct TSS_Com_Class *com, const struct TSS_Header_Info *header_info, struct TSS_Header *out);

TSS_API int tssPeekCommandChecksum(const struct TSS_Com_Class *com, uint16_t start, uint16_t len);
TSS_API int tssPeekValidateCommand(const struct TSS_Com_Class *com, 
    uint8_t header_size, uint16_t header_len_field, uint8_t header_checksum_field, size_t min_data_len, size_t max_data_len);

//Setting Helpers
TSS_API size_t tssBuildGetSettingsString(char *out, size_t out_size, uint16_t count, ...);
TSS_API size_t tssBuildGetSettingsStringV(char *out, size_t out_size, uint16_t count, va_list args);
TSS_API size_t tssBuildGetSettingsStringBinary(uint8_t *out, size_t out_size, uint16_t count, ...);

enum TSS_SettingsCallbackState {
    TSS_SettingsCallbackStateError = -1,
    TSS_SettingsCallbackStateIgnored = 0,
    TSS_SettingsCallbackStateProcessed = 1
};

struct TSS_GetSettingsCallbackInfo {
    const struct TSS_Com_Class *com;
    const struct TSS_Setting *setting;
    const char *key;
    uint8_t *checksum;
};

typedef enum TSS_SettingsCallbackState (*TssGetSettingsCallback)(struct TSS_GetSettingsCallbackInfo info, void *user_data);

TSS_API int tssGetSettingsWrite(const struct TSS_Com_Class *com, bool header, const char *key_string);
TSS_API int tssGetSettingsRead(const struct TSS_Com_Class *com, ...);
TSS_API int tssGetSettingsReadV(const struct TSS_Com_Class *com, va_list args);
TSS_API int tssGetSettingsReadCb(const struct TSS_Com_Class *com, TssGetSettingsCallback callback, void *user_data);

TSS_API int tssSetSettingsWrite(const struct TSS_Com_Class *com, bool header, const char **keys, uint8_t num_keys, const void **data);
TSS_API int tssSetSettingsRead(const struct TSS_Com_Class *com, struct TSS_Setting_Response *response);

//The settings header is actually just an ID appended to the 
//beggining of the response to help with framing
TSS_API int tssReadSettingsHeader(const struct TSS_Com_Class *com, uint32_t *id);
TSS_API int tssPeekSettingsHeader(const struct TSS_Com_Class *com, uint32_t *id);

//Low Level Reading Functions
TSS_API int tssReadParams(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, ...);
TSS_API int tssReadParamsVp(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, va_list *args);

TSS_API int tssReadParamsChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum);
TSS_API int tssReadBytesChecksumOnly(const struct TSS_Com_Class *com, size_t num_bytes, uint8_t *checksum);

#ifdef __cplusplus
}
#endif

#endif /* __TSS_API_H__ */
