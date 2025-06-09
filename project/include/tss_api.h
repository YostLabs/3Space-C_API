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
#include <stdarg.h>

#include "tss_com_class.h"
#include "tss_command.h"
#include "tss_header.h"

int tssWriteCommand(const struct TSS_Com_Class *com, bool header, const struct TSS_Command *command, const void **data);

//Returns -Error or >= 0 checksum
int tssReadCommand(const struct TSS_Com_Class *com, const struct TSS_Command *command, ...);
int tssReadCommandV(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list args);
int tssReadCommandVp(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list *args);
int tssReadCommandChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Command *command);


int tssReadHeader(const struct TSS_Com_Class *com, const struct TSS_Header_Info *header_info, struct TSS_Header *out);

//Setting Helpers
size_t tssBuildGetSettingsString(char *out, size_t out_size, uint16_t count, ...);
size_t tssBuildGetSettingsStringV(char *out, size_t out_size, uint16_t count, va_list args);
size_t tssBuildGetSettingsStringBinary(uint8_t *out, size_t out_size, uint16_t count, ...);

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

int tssGetSettingsWrite(const struct TSS_Com_Class *com, const char *key_string);
int tssGetSettingsRead(const struct TSS_Com_Class *com, ...);
int tssGetSettingsReadV(const struct TSS_Com_Class *com, va_list args);
int tssGetSettingsReadCb(const struct TSS_Com_Class *com, TssGetSettingsCallback callback, void *user_data);

int tssSetSettingsWrite(const struct TSS_Com_Class *com, const char **keys, uint8_t num_keys, const void **data);
int tssSetSettingsRead(const struct TSS_Com_Class *com, struct TSS_Setting_Response *response);

//Low Level Reading Functions
int tssReadParams(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, ...);
int tssReadParamsVp(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, va_list *args);

int tssReadParamsChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum);
int tssReadBytesChecksumOnly(const struct TSS_Com_Class *com, size_t num_bytes, uint8_t *checksum);

#endif /* __TSS_API_H__ */
