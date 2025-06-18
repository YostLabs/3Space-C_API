
#include "tss/api/core.h"
#include "tss/api/command.h"

#include "tss/constants.h"
#include "tss/errors.h"
#include "tss/sys/config.h"
#include "tss/sys/endian.h"

#include "tss/sys/string.h"
#include <stdarg.h>

//---------------------------------PROTOTYPES-------------------------------------
inline static void send_params(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, const void ***raw_data, uint8_t *checksum);
inline static void send_param(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, const uint8_t *raw_data, uint8_t *checksum);
inline static void swap_singular_param_endianess(uint8_t *data, const struct TSS_Param *param);
inline static void swap_param_endianess(uint8_t *data, const struct TSS_Param *param);

//------------------------------API FUNCTIONS---------------------------------

//NOTE: The reason we don't use a variadic write function is because types are auto promoted. EX: floats become doubles.
//This is undesirable as this works via the raw bytes not the types, and so it is avoided by using a void** instead.
int tssWriteCommand(const struct TSS_Com_Class *com, bool header, const struct TSS_Command *command, const void **data)
{   
    const struct TSS_Param *cur_param;
    uint8_t start_byte, checksum, element_index, i;

    checksum = command->num;
    start_byte = (header) ? TSS_BINARY_HEADER_START_BYTE : TSS_BINARY_START_BYTE;

    TSS_COM_BEGIN_WRITE(com);
    com->out.write(&start_byte, 1, com->user_data);
    com->out.write(&command->num, 1, com->user_data);

    if(command->in_format != NULL) {
        send_params(com, command->in_format, &data, &checksum);
    }

    com->out.write(&checksum, 1, com->user_data);
    TSS_COM_END_WRITE(com);
    return 0;
}

int tssReadCommand(const struct TSS_Com_Class *com, const struct TSS_Command *command, ...)
{
    va_list args;
    int result;

    va_start(args, command);
    result = tssReadCommandV(com, command, args);
    va_end(args);

    return result;
}

int tssReadCommandV(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list args)
{
    return tssReadCommandVp(com, command, &args);
}

int tssReadCommandVp(const struct TSS_Com_Class *com, const struct TSS_Command *command, va_list *args)
{
    uint8_t checksum;
    int err;
    if(command->out_format == NULL) return 0;
    checksum = 0;
    err = tssReadParamsVp(com, command->out_format, &checksum, args);
    if(err) return err;
    return checksum;
}

int tssReadCommandChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Command *command)
{
    uint8_t checksum;
    int err;
    if(command->out_format == NULL) return 0;
    checksum = 0;
    err = tssReadParamsChecksumOnly(com, command->out_format, &checksum);
    if(err) return err;
    return checksum;
}

int tssReadHeader(const struct TSS_Com_Class *com, const struct TSS_Header_Info *header_info, struct TSS_Header *out)
{
    uint8_t data[TSS_HEADER_MAX_SIZE];
    if(header_info->size == 0) return 0;
    if(com->in.read(header_info->size, data, com->user_data) != header_info->size) {
        return TSS_ERR_READ;
    }
    
    tssHeaderFromBytes(header_info, data, out);
    return 0;
}

int tssPeekHeader(const struct TSS_Com_Class *com, const struct TSS_Header_Info *header_info, struct TSS_Header *out)
{
    uint8_t data[TSS_HEADER_MAX_SIZE];
    if(header_info->size == 0) {
        return 0;
    }
    if(com->in.peek(0, header_info->size, data, com->user_data) != header_info->size) {
        return TSS_ERR_READ_LEN;
    }
    
    tssHeaderFromBytes(header_info, data, out);
    return 0;
}

int tssPeekValidateCommand(const struct TSS_Com_Class *com, 
    uint8_t header_size, uint16_t header_len_field, uint8_t header_checksum_field, size_t min_data_len, size_t max_data_len) 
{
    int checksum;

    //This is to prevent situations where the data_len field is corrupted to
    //a really large values and causes read timeouts to frequently occur, essentially
    //stalling validation, or when a lot of zeroes are sent causing a data_len of 0
    //and checksum of 0 which gets validated successfully.
    if(header_len_field > max_data_len || header_len_field < min_data_len) {
        return TSS_ERR_UNEXPECTED_PACKET_LENGTH;
    }

    //Validate the checksum
    checksum = tssPeekCommandChecksum(com, header_size, header_len_field);
    if(checksum < 0) { //Error
        return checksum;
    }

    if(checksum != header_checksum_field) {
        return TSS_ERR_CHECKSUM_MISMATCH;
    }

    return TSS_SUCCESS;
}

int tssPeekCommandChecksum(const struct TSS_Com_Class *com, uint16_t start, uint16_t len) {
    uint16_t i, read_len;
    uint8_t j, checksum, data[40]; //Read 40 bytes at a time
    int num_read;

    checksum = 0;
    for(i = 0; i < len; i+= sizeof(data)) {
        //Figure out how much to peek
        read_len = len - i;
        if(sizeof(data) < read_len) {
            read_len = sizeof(data);
        }

        //Peek in that amount, and validate it was successful
        num_read = com->in.peek(i + start, read_len, data, com->user_data);
        if(num_read != read_len) {
            if(num_read < 0) {
                return num_read;
            }
            return TSS_ERR_READ_LEN;
        }

        //Add to the checksum
        for(j = 0; j < num_read; j++) {
            checksum += data[j];
        }
    }

    return checksum;
} 

//------------------------------------------SETTINGS PROTOCOL----------------------------------------------

//The binary settings protocol is built on top of the ascii protocol, so some helper functions are broken out into
//ascii versions. However, these are not directly exposed as it is the intention for the user to utilize the more efficent
//binary versions.

/// @brief Creates a getSettings string in the given buffer using the given variable string keys.
/// @param out The buffer to store the result in
/// @param out_size The max size of out
/// @param count The number of string keys passed in
/// @param args count string keys
/// @return The required/resulting size of the out string, not counting the null terminator
/// @note This gurantees to produce a null terminated string. If the return value >= out_size, then a large enough
/// buffer was not supplied and the resulting string is truncated early.
size_t tssBuildGetSettingsStringV(char *out, size_t out_size, uint16_t count, va_list args)
{
    size_t key_len, out_len;
    char *key;

    uint16_t i, param_i;

    out_len = 0;
    for(param_i = 0; param_i < count; param_i++) {
        key = va_arg(args, char*);
        key_len = strlen(key);
        
        if(param_i > 0) { //Insert Separator
            if(out_len++ < (out_size - 1)) {
                *out++ = ';';
            }
        }

        for(i = 0; i < key_len; i++) { //Insert Key
            if(out_len++ < (out_size - 1))
            {
                *out++ = *key++;
            }
        }
    }

    //Insert Null Terminator
    *out = '\0';
    return out_len;
}

size_t tssBuildGetSettingsString(char *out, size_t out_size, uint16_t count, ...)
{
    va_list args;
    size_t result;

    va_start(args, count);
    result = tssBuildGetSettingsStringV(out, out_size, count, args);
    va_end(args);

    return result;
}

/// @brief Creates a getSettings binary string in the given buffer using the given variable string keys.
/// @param out The buffer to store the result in
/// @param out_size The max size of out
/// @param count The number of string keys passed in
/// @param args count string keys
/// @return The required/resulting size of the out string, not counting the null terminator
/// @note This gurantees to produce a null terminated string. If the return value > out_size, then a large enough
/// buffer was not supplied and the resulting string is truncated early.
/// @note The final string in the base output will be guranteed null terminated.
size_t tssBuildGetSettingsStringBinary(uint8_t *out, size_t out_size, uint16_t count, ...)
{
    va_list args;
    size_t out_len;
    uint8_t checksum;

    //Binary get string is identical except the null terminator is part of the data and a checksum is appended.
    va_start(args, count);
    out_len = tssBuildGetSettingsStringV((char*)out, out_size, count, args);
    va_end(args);

    //Compute the checksum
    while(*out != '\0') checksum += *out++;

    //Add the null terminator as part of the data
    out_len++;
    out++; //Advance past the null terminator for adding the checksum in

    //Add the checksum if room
    if(out_len++ < out_size) {
        *out = checksum;
    }

    return out_len;
}

int tssGetSettingsWrite(const struct TSS_Com_Class *com, bool header, const char *key_string)
{
    const uint8_t start_byte = (header) ? TSS_BINARY_READ_SETTINGS_HEADER_START_BYTE : TSS_BINARY_READ_SETTINGS_START_BYTE;

    size_t key_len;
    uint8_t checksum;
    
    key_len = strlen(key_string);
    if(key_len > TSS_MAX_CMD_LEN-2) { //-2 for room for null terminator and checksum
        return -1; 
    }

    checksum = 0;
    for(size_t i = 0; i < key_len; i++) {
        checksum += key_string[i];
    }

    TSS_COM_BEGIN_WRITE(com);
    com->out.write(&start_byte, 1, com->user_data);
    com->out.write((uint8_t*)key_string, key_len+1, com->user_data); //+1 to send the  null terminator
    com->out.write(&checksum, 1, com->user_data);
    TSS_COM_END_WRITE(com);

    return 0;
}

#include <stdio.h>
int tssGetSettingsReadCb(const struct TSS_Com_Class *com, TssGetSettingsCallback callback, void *user_data) {
    char buffer[TSS_MAX_SETTINGS_KEY_LEN];
    struct TSS_GetSettingsCallbackInfo cb_info;
    enum TSS_SettingsCallbackState cb_state;
    const char *key;
    const struct TSS_Setting *setting;
    uint8_t len, checksum, num_read, i;
    uint16_t param_size;
    bool end_reached;
    int err;
    
    //Initialize unchanging values
    cb_info.checksum = &checksum;
    cb_info.com = com;

    checksum = 0;
    num_read = 0;
    end_reached = false;
    while(!end_reached) {
        len = com->in.read_until('\0', buffer, sizeof(buffer), com->user_data);
        key = buffer;

        //Failed to read or find key
        if(len == 0 || key[len-1] != '\0') {
            return TSS_ERR_READ;
        }

        setting = tssGetSetting(key);
        if(setting == NULL) { //Unregistered key
            if(strcmp(key, TSS_SETTING_KEY_ERR_STRING) == 0) {
                //Key Error. Unkown how to continue parsing, return
                printf("Invalid Key!\n");
                return TSS_ERR_SETTING_KEY_INVALID;    
            }
            printf("Unregistered Key: %s\n", key);
            return TSS_ERR_SETTING_KEY_UNREGISTERED;
        }
        if(setting->out_format == NULL) {
            return TSS_ERR_INVALID_READ_KEY;
        }

        //Add key to the checksum
        while(*key != '\0') {
            checksum += *key++;
        }
        key = buffer; //Reset key back to the start

        //Send the setting to the callback, if not processed, process the bytes here (compute checksum and discard data)
        cb_info.key = key;
        cb_info.setting = setting;
        cb_state = callback(cb_info, user_data);
        if(cb_state == TSS_SettingsCallbackStateIgnored) {
            err = tssReadParamsChecksumOnly(com, setting->out_format, &checksum);
            if(err) return err;
        }
        else if(cb_state == TSS_SettingsCallbackStateError) {
            return TSS_ERR_GET_SETTING_CALLBACK;
        }
        num_read++;

        //Read in the byte that determines if done or more to parse
        len = com->in.read(1, buffer, com->user_data);
        if(len != 1) return TSS_ERR_READ;

        //Process that byte
        checksum += buffer[0];
        if(buffer[0] == '\0') {
            end_reached = true;
        }
        else if(buffer[0] != TSS_SETTING_SEPARATOR) { //Unknown parsing character
            return TSS_ERR_UNEXPECTED_CHARACTER;
        }
    }

    //Read in the checksum and validate
    len = com->in.read(1, buffer, com->user_data);
    if(len != 1) {
        return TSS_ERR_READ;
    }

    if((uint8_t)buffer[0] != checksum) {
        printf("Checksum Mismatch: 0x%02x != 0x%02x\n", (uint8_t)buffer[0], checksum);
        return TSS_ERR_CHECKSUM_MISMATCH;
    }

    return num_read;
}

struct GetSettingUserData {
    va_list *args;
    int result;
};

static enum TSS_SettingsCallbackState getSettingsCallback(
    struct TSS_GetSettingsCallbackInfo info, void *user_data)
{
    struct GetSettingUserData *user = user_data;

    user->result = tssReadParamsVp(info.com, info.setting->out_format, info.checksum, user->args);
    if(user->result != TSS_SUCCESS) {
        return TSS_SettingsCallbackStateError;
    }
    return TSS_SettingsCallbackStateProcessed;
}

int tssGetSettingsRead(const struct TSS_Com_Class *com, ...)
{
    va_list args;
    int result;
    va_start(args, com);
    result = tssGetSettingsReadV(com, args);
    va_end(args);

    return result;
}

int tssGetSettingsReadV(const struct TSS_Com_Class *com, va_list args)
{
    int result;
    struct GetSettingUserData user_data = {
        .args = &args,
        .result = TSS_SUCCESS
    };

    result = tssGetSettingsReadCb(com, getSettingsCallback, &user_data);
    return (result != TSS_ERR_GET_SETTING_CALLBACK) ? result : user_data.result;
}

int tssSetSettingsWrite(const struct TSS_Com_Class *com, bool header, const char **keys, uint8_t num_keys, const void **data)
{
    const static uint8_t separator = TSS_SETTING_SEPARATOR;
    const uint8_t start_byte = (header) ? TSS_BINARY_WRITE_SETTINGS_HEADER_START_BYTE : TSS_BINARY_WRITE_SETTINGS_START_BYTE;

    const struct TSS_Setting *setting;
    uint8_t key_index, checksum;
    const char *key;

    TSS_COM_BEGIN_WRITE(com);
    com->out.write(&start_byte, 1, com->user_data);

    checksum = 0;
    for(key_index = 0; key_index < num_keys; key_index++) {
        key = keys[key_index];
        setting = tssGetSetting(key);
        if(setting == NULL) {
            return TSS_ERR_SETTING_KEY_INVALID;
        }
        if(setting->in_format == NULL) {
            return TSS_ERR_INVALID_WRITE_KEY;
        }

        com->out.write((uint8_t*)key, strlen(key) + 1, com->user_data);
        while(*key != '\0') checksum += *key++; //Add key to the checksum

        send_params(com, setting->in_format, &data, &checksum);

        if(key_index < num_keys - 1) { //Insert the go next character
            com->out.write(&separator, 1, com->user_data);
            checksum += separator;
        }
    }

    //Done writing, now add the null terminator and checksum
    com->out.write((uint8_t*)"\0", 1, com->user_data);
    com->out.write(&checksum, 1, com->user_data);
    TSS_COM_END_WRITE(com);

    return TSS_SUCCESS;
}

int tssSetSettingsRead(const struct TSS_Com_Class *com, struct TSS_Setting_Response *response)
{
    uint8_t data[TSS_SETTING_RESPONSE_SIZE];
    uint8_t len;

    len = com->in.read(TSS_SETTING_RESPONSE_SIZE, data, com->user_data);
    if(len != TSS_SETTING_RESPONSE_SIZE) {
        return TSS_ERR_READ;
    }

    //Check checksum
    if(((data[0] + data[1]) % 256) != data[2]) {
        response->error = TSS_ERR_CHECKSUM_MISMATCH;
        response->num_success = 0;
        return TSS_ERR_CHECKSUM_MISMATCH;
    }

    *response = (struct TSS_Setting_Response) {
        .error = data[0],
        .num_success = data[1]
    };

    return TSS_SUCCESS;
}


int tssReadSettingsHeader(const struct TSS_Com_Class *com, uint32_t *id) {
    int result;
    result = com->in.read(TSS_BINARY_SETTINGS_ID_SIZE, (uint8_t*)id, com->user_data);
#if TSS_ENDIAN_CONFIG == TSS_ENDIAN_BIG
    swap_endianess((uint8_t*)id, TSS_BINARY_SETTINGS_ID_SIZE);
#endif
    return result;
}

int tssPeekSettingsHeader(const struct TSS_Com_Class *com, uint32_t *id) {
    int result;
    result = com->in.peek(0, TSS_BINARY_SETTINGS_ID_SIZE, (uint8_t*)id, com->user_data);
#if TSS_ENDIAN_CONFIG == TSS_ENDIAN_BIG
    swap_endianess((uint8_t*)id, TSS_BINARY_SETTINGS_ID_SIZE);
#endif
    return result;
}

//-----------------------HELPER FUNCTIONS------------------------------

inline static void send_params(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, const void ***raw_data, uint8_t *checksum)
{
    while(!TSS_PARAM_IS_NULL(cur_param)) {
        send_param(com, cur_param, **raw_data, checksum);

        cur_param++;
        (*raw_data)++; 
    }
}

inline static void send_param(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, const uint8_t *raw_data, uint8_t *checksum)
{
    uint8_t element, conversion[8]; //Max U64 is 8 bytes
    uint8_t i;
    uint16_t param_len;
    bool is_str;

    is_str = TSS_PARAM_IS_STRING(cur_param);
    param_len = (is_str) ? strlen(raw_data) + 1 : cur_param->size * cur_param->count;

    for(i = 0; i < param_len; i++) {
        *checksum += raw_data[i];
    }

    if(TSS_ENDIAN_IS_LITTLE) {
        com->out.write(raw_data, param_len, com->user_data);
    }
    else {
        //Have to send each part of the param 1 at a time because need
        //to swap endianess, but the incoming data is const, so not allowed to modify it there
        for(element = 0; element < cur_param->count; element++) {
            if(is_str) { //Strings don't need endianess swapped
                com->out.write(raw_data, param_len, com->user_data);
            }
            else {
                //Swap endianess
                for(i = 0; i < cur_param->size; i++) {
                    conversion[i] = raw_data[cur_param->size-1-i];
                }

                com->out.write(conversion, cur_param->size, com->user_data);
            }

            //Advance to the next element
            raw_data += cur_param->size;
        }
    }
}

//Swaps a singular element. So if an array of floats, just swaps 1 float
inline static void swap_singular_param_endianess(uint8_t *data, const struct TSS_Param *param) {
    tssSwapEndianess(data, param->size);
}

//Swaps all elements. So if an array of floats, swaps all the floats.
inline static void swap_param_endianess(uint8_t *data, const struct TSS_Param *param) {
    uint8_t element;

    for(element = 0; element < param->count; element++) {
        swap_singular_param_endianess(data, param);
        data += param->size;
    }
}

int tssReadParams(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, ...)
{
    va_list args;
    int result;
    va_start(args, checksum);
    result = tssReadParamsVp(com, cur_param, checksum, &args);
    va_end(args);
    return result;
}

int tssReadParamsVp(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum, va_list *args)
{
    uint32_t str_len, len, i;
    
    while(!TSS_PARAM_IS_NULL(cur_param)) {
        uint8_t *out = (uint8_t*) va_arg(*args, void*);

        if(TSS_PARAM_IS_STRING(cur_param)) {
            //When using string width specifier, the value MUST be 32 bits. EX: 4ul;
            str_len = va_arg(*args, uint32_t);
            len = com->in.read_until('\0', out, str_len, com->user_data);

            if(len == 0) {
                if(str_len > 0) out[0] = '\0';
                return TSS_ERR_READ; 
            }

            if(out[len-1] != '\0') {
                //Force the string to be null terminated
                out[str_len-1] = '\0';
                return TSS_ERR_BUFFER_OVERFLOW;
            }
        }
        else {
            len = com->in.read(cur_param->count * cur_param->size, out, com->user_data);
#if TSS_ENDIAN_CONFIG == TSS_ENDIAN_BIG
            swap_param_endianess(out, cur_param);
#endif
            if(len != cur_param->count * cur_param->size) {
                return -TSS_ERR_READ;
            }
        }

        for(i = 0; i < len; i++) {
            *checksum += out[i];
        }
        cur_param++;
    }

    return TSS_SUCCESS; 
}

int tssReadParamsChecksumOnly(const struct TSS_Com_Class *com, const struct TSS_Param *cur_param, uint8_t *checksum)
{
    uint8_t buffer[40]; //Will read at most 40 bytes at a time
    uint16_t param_size, len;
    uint8_t i;

    while(!TSS_PARAM_IS_NULL(cur_param)) {
        if(TSS_PARAM_IS_STRING(cur_param)) {
            do {
                len = com->in.read_until('\0', buffer, sizeof(buffer), com->user_data);
                if(len == 0) {
                    return TSS_ERR_READ;
                }
                for(i = 0; i < len; i++) {
                    *checksum += buffer[i];
                }
            } while(buffer[len-1] != '\0');
        }
        else {
            param_size = cur_param->count * cur_param->size;
            while(param_size > 0) {
                len = (param_size < sizeof(buffer)) ? param_size : sizeof(buffer);
                len = com->in.read(len, buffer, com->user_data);
                if(len == 0) {
                    return TSS_ERR_READ;
                }
                for(i = 0; i < len; i++) {
                    *checksum += buffer[i];
                }
                param_size -= len;
            }
        }
        cur_param++;
    }
}

int tssReadBytesChecksumOnly(const struct TSS_Com_Class *com, size_t num_bytes, uint8_t *checksum)
{
    uint8_t buffer[40]; //Will read at most 40 bytes at a time
    uint16_t read_len, num_read;
    uint8_t i;

    while(num_bytes > 0) {
        read_len = (num_bytes > sizeof(buffer)) ? sizeof(buffer) : num_bytes;
        num_read = com->in.read(read_len, buffer, com->user_data);
        if(read_len != num_read) {
            return TSS_ERR_READ;
        }
        if(checksum != NULL) {
            for(i = 0; i < num_read; i++) {
                *checksum += buffer[i];
            }
        }

    }

    return TSS_SUCCESS;
}