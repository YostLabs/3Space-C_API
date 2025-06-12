/*
*  Contains the regular sensor functionality/managed functionality
*/
#include "tss_config.h"
#if !TSS_MINIMAL_SENSOR
#include "tss_sensor.h"
#include "tss_sensor_internal.h"
#include "tss_api.h"
#include "tss_string.h"
#include "tss_util.h"
#include "tss_errors.h"

#define REQUIRED_HEADER_BITS (TSS_HEADER_CHECKSUM_BIT | TSS_HEADER_LENGTH_BIT | TSS_HEADER_ECHO_BIT)

#define THREESPACE_AWAIT_COMMAND_FOUND 0
#define THREESPACE_AWAIT_COMMAND_TIMEOUT 1
#define THREESPACE_AWAIT_BOOTLOADER 2

#define THREESPACE_UPDATE_COMMAND_PARSED 0
#define THREESPACE_UPDATE_COMMAND_MISALIGNED 1
#define THREESPACE_UPDATE_COMMAND_NOT_ENOUGH_DATA 2

void createTssSensor(TSS_Sensor *sensor, struct TSS_Com_Class *com)
{
    *sensor = (TSS_Sensor) {
        .com = com
    };
}

//----------------------------------CORE SENSOR FUNCTIONS------------------------------------------------------

static void cacheHeader(TSS_Sensor *sensor);
static void cacheStreamSlots(TSS_Sensor *sensor);

static void initFirmware(TSS_Sensor *sensor);

//----------------------------------------ALIGNMENT & VALIDATION FUNCTIONS-----------------------------------------
static inline void handleMisalignment(TSS_Sensor *sensor);
static int peekValidatePacket(TSS_Sensor *sensor, const struct TSS_Header *header, size_t min_data_len, size_t max_data_len);
static struct TSS_Header* tryPeekHeader(TSS_Sensor *sensor, struct TSS_Header *out);

static int internalUpdate(TSS_Sensor *sensor, const struct TSS_Header *header);
static int awaitCommandResponse(TSS_Sensor *sensor, uint8_t cmd_num, uint16_t min_data_len, uint16_t max_data_len);
static int awaitGetSettingResponse(TSS_Sensor *sensor, uint16_t min_len, bool check_bootloader);
static int awaitSetSettingResponse(TSS_Sensor *sensor, uint16_t num_keys);

void initTssSensor(TSS_Sensor *sensor) {
    sensorInternalForceStopStreaming(sensor);

    //Clear out any garbage data
    sensor->com->in.clear_timeout(sensor->com->user_data, 5);

    //Check for bootloader
    sensor->_in_bootloader = false; //TODO
    if(!sensor->_in_bootloader) {
        initFirmware(sensor);
    }
}


static void initFirmware(TSS_Sensor *sensor) {
    sensor->dirty = false;
    sensorUpdateCachedSettings(sensor);
}

//----------------------------------CACHEING FUNCTIONS-------------------------------------------

static void cacheHeader(TSS_Sensor *sensor) {
    uint8_t header;
    sensorReadHeader(sensor, &header);
    if(header == sensor->header_cfg.bitfield) return; //Nothing to update

    //Do not allow changing the header if currently streaming to prevent misalignment issues
    if(sensorIsStreaming(sensor)) {
        sensorWriteHeader(sensor, sensor->header_cfg.bitfield);
        return;
    }

    //Force required bits to be enabled
    if((header & REQUIRED_HEADER_BITS) != REQUIRED_HEADER_BITS) {
        header |= REQUIRED_HEADER_BITS;
        sensorWriteHeader(sensor, header);
    }
    sensor->header_cfg = tssHeaderInfoFromBitfield(header);
}

static void cacheStreamSlots(TSS_Sensor *sensor) {
    char stream_slots[130];
    uint16_t output_size, size;
    uint8_t i;
    
    sensorReadStreamSlots(sensor, stream_slots, sizeof(stream_slots));
    tssUtilStreamSlotStringToCommands(stream_slots, sensor->streaming.data.commands);
    
    output_size = 0;
    for(i = 0; i < TSS_NUM_STREAM_SLOTS && sensor->streaming.data.commands[i] != NULL; i++) {
        tssGetParamListSize(sensor->streaming.data.commands[i]->out_format, &size, &size);
        output_size += size;
    }
    sensor->streaming.data.output_size = output_size;
}

void sensorUpdateCachedSettings(TSS_Sensor *sensor) {
    uint8_t value;
    
    sensor->dirty = false;

    //Cache the header
    cacheHeader(sensor);

    //Cache debug mode
    sensorReadDebugMode(sensor, &value);
    sensor->_immediate_debug = value;

    cacheStreamSlots(sensor);
}

static void checkDirty(TSS_Sensor *sensor) {
    if(!sensor->dirty) return;

    sensorInternalForceStopStreaming(sensor);

    //TODO: Add bootloader logic
    initFirmware(sensor);
    
    //This is set in a lot of places just to ensure
    //no matter what function gets called, the dirty
    //state is properly updated. In general, this doesn't
    //actually need to be here due to initFirmware clearing
    //the dirty flag itself, but this adds to readability
    sensor->dirty = false;
}

//--------------------------------GENERIC FUNCTIONS-------------------------------------

int sensorInternalExecuteCommandCustomV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, va_list outputs)
{
    int err_or_checksum;
    checkDirty(sensor);
    tssWriteCommand(sensor->com, sensor->_header_enabled, command, input);
    err_or_checksum = read_func(sensor, command, outputs);
    if(err_or_checksum < 0) return err_or_checksum; //Return the error
    return TSS_SUCCESS; //No error
}

int sensorInternalBaseCommandRead(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs)
{
    int result;
    uint16_t min_size, max_size;
    tssGetParamListSize(command->out_format, &min_size, &max_size);
    result = awaitCommandResponse(sensor, command->num, min_size, max_size);
    if(result != THREESPACE_AWAIT_COMMAND_FOUND) {
        return TSS_RESPONSE_NOT_FOUND;
    }
    sensorInternalHandleHeader(sensor);
    return tssReadCommandV(sensor->com, command, outputs);
}

int sensorInternalProcessStreamingBatch(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs)
{
    int result;
    result = awaitCommandResponse(sensor, command->num, sensor->streaming.data.output_size, sensor->streaming.data.output_size);
    if(result != THREESPACE_AWAIT_COMMAND_FOUND) {
        return TSS_RESPONSE_NOT_FOUND;
    }
    sensorInternalHandleHeader(sensor);
    return sensorInternalReadStreamingBatch(sensor, command, outputs);
}

//Common to both read setting functions
inline static int baseReadSettings(TSS_Sensor *sensor, const char *key_string)
{
    uint32_t id;
    uint16_t min_response_len;
    checkDirty(sensor);
    tssGetSettingsWrite(sensor->com, true, key_string);

    //+1 is because the terminating character/delimiter is also required in the response.
    min_response_len = str_len_until(key_string, ';') + 1 + TSS_BINARY_SETTINGS_ID_SIZE;
    if(min_response_len > TSS_SETTING_KEY_ERR_STRING_LEN + 1 + TSS_BINARY_SETTINGS_ID_SIZE) {
        min_response_len = TSS_SETTING_KEY_ERR_STRING_LEN + 1 + TSS_BINARY_SETTINGS_ID_SIZE;
    }
    if(awaitGetSettingResponse(sensor, min_response_len, false) != THREESPACE_AWAIT_COMMAND_FOUND) {
        return TSS_RESPONSE_NOT_FOUND;
    }  

    tssReadSettingsHeader(sensor->com, &id);
    return TSS_SUCCESS;
}

int sensorReadSettingsV(TSS_Sensor *sensor, const char *key_string, va_list outputs)
{
    int result;
    result = baseReadSettings(sensor, key_string);
    if(result < 0) {
        return result;
    }
    return tssGetSettingsReadV(sensor->com, outputs);
}

int sensorReadSettingsQuery(TSS_Sensor *sensor, const char *key_string, TssGetSettingsCallback cb, void *user_data)
{
    int result;
    result = baseReadSettings(sensor, key_string);
    if(result < 0) {
        return result;
    }
    return tssGetSettingsReadCb(sensor->com, cb, user_data);
}

inline static int keyInArray(const char *key, const char * const *array, uint8_t size)
{
    uint16_t i = 0;
    for(i = 0; i < size; i++) {
        if(tssSettingKeyCmp(key, array[i]) == 0) {
            return i;
        }
    }
    return -1;
}

inline static void checkAndCacheDebugMode(TSS_Sensor *sensor, const char **keys, 
    uint8_t num_keys, const void **data) 
{
    const struct TSS_Setting *setting;
    const struct TSS_Param *param;
    uint16_t i;
    uint8_t value;

    for(i = 0; i < num_keys; i++) {
        if(tssSettingKeyCmp(keys[i], "debug_mode") == 0) {
            value = *((uint8_t*)(*data));
            sensor->_immediate_debug = (value == 1);
            return;
        }
        else {
            //Advance the data by params input length so that can get the value associated with the debug key when found
            setting = tssGetSetting(keys[i]);
            if(setting == NULL) return;
            param = setting->in_format;
            while(!TSS_PARAM_IS_NULL(param)) {
                data++;
                param++;
            }
        }
    }
}

static const char * const K_HEADER_KEYS[] = { "header", "header_status", "header_timestamp", "header_echo", "header_checksum", "header_serial", "header_length" }; 
int sensorWriteSettings(TSS_Sensor *sensor, const char **keys, uint8_t num_keys, 
    const void **data)
{
    int result;
    uint32_t id;
    uint16_t i;

    checkDirty(sensor);

    //Must check for debug_mode=1 before sending the change to be able to properly handle
    //reading the response (since debug messages may be output immediately before the response happens)
    checkAndCacheDebugMode(sensor, keys, num_keys, data);

    tssSetSettingsWrite(sensor->com, true, keys, num_keys, data);

    if(awaitSetSettingResponse(sensor, num_keys) != THREESPACE_AWAIT_COMMAND_FOUND) {
        return TSS_RESPONSE_NOT_FOUND;
    }

    tssReadSettingsHeader(sensor->com, &id);
    result = tssSetSettingsRead(sensor->com, &sensor->last_write_setting_response);

    //Check for keys that may need cacheing. This is done here to allow the user to not have
    //to manually call updateCachedSettings when using the base API. Speed is not really a concern
    //for the settings protocol either since changes should be infrequent.

    if(keyInArray("default", keys, num_keys) >= 0) {
        sensorUpdateCachedSettings(sensor);
    }
    else {
        //Check for header keys
        for(i = 0; i < num_keys; i++) {
            if(keyInArray(keys[i], K_HEADER_KEYS, sizeof(K_HEADER_KEYS) / sizeof(K_HEADER_KEYS[0])) >= 0) {
                cacheHeader(sensor);
                break;
            }
        }

        //Check for stream slots
        if(keyInArray("stream_slots", keys, num_keys) >= 0) {
            cacheStreamSlots(sensor);
        }
    }

    if(result < 0) {
        printf("Failed to write: %s %d\n", keys[0], result);
    }

    return result;
}

int sensorUpdateStreaming(TSS_Sensor *sensor)
{
    struct TSS_Header header;
    tss_time_t start_time;
    int result;
    checkDirty(sensor);
    
    start_time = sensor->com->time.get();
    //Update until either a command is successfully parsed or not enough data for a command
    //Mainly just don't want to have to call this function multiple times to fix misalignments.
    do {
        if(sensor->com->in.length(sensor->com->user_data) < sensor->header_cfg.size) {
            return false;
        }

        tssPeekHeader(sensor->com, &sensor->header_cfg, &header);
        result = internalUpdate(sensor, &header);
    } while(result == THREESPACE_UPDATE_COMMAND_MISALIGNED && 
            sensor->com->time.diff(start_time) < sensor->com->in.get_timeout(sensor->com->user_data));

    return result == THREESPACE_UPDATE_COMMAND_PARSED;
}

//-------------------------------------------AWAITING/ALIGNMENT FUNCTIONS--------------------------------------------

static inline void handleMisalignment(TSS_Sensor *sensor) {
    //Continously read 1 byte until aligned
    uint8_t tmp;
    sensor->com->in.read(1, &tmp, sensor->com->user_data);
}


/// @brief Fast fail version of peeking a header that returns a pointer to the out
/// structure or NULL if no header to peek.
static struct TSS_Header* tryPeekHeader(TSS_Sensor *sensor, struct TSS_Header *out) {
    int err;
    if(sensor->com->in.length(sensor->com->user_data) < sensor->header_cfg.size) {
        return NULL;
    }
    err = tssPeekHeader(sensor->com, &sensor->header_cfg, out);
    if(err) {
        return NULL;
    }
    return out;
}

static int peekValidatePacket(TSS_Sensor *sensor, const struct TSS_Header *header, size_t min_data_len, size_t max_data_len)
{
    int err;
    err = tssPeekValidateCommand(sensor->com, sensor->header_cfg.size, 
        header->length, header->checksum, min_data_len, max_data_len);
    if(err == TSS_ERR_INSUFFICIENT_BUFFER) {
        //Pretend this is a success, the com class internal buffer
        //is not big enough to validate. The validation will be performed
        //by the read operation. This does mean if it actually is invalid
        //more data will be lost then normal.
        //TODO: Add a warning mechanism here.
        return TSS_SUCCESS; 
    }
    return err;
}

static int awaitCommandResponse(TSS_Sensor *sensor, uint8_t cmd_num, uint16_t min_data_len, uint16_t max_data_len) {
    int err;
    tss_time_t start_time;
    struct TSS_Header header;

    //Nothing to do but pretend it was found. Can't check if header isn't enabled.
    if(!sensor->_header_enabled) return THREESPACE_AWAIT_COMMAND_FOUND;

    start_time = sensor->com->time.get();
    while(sensor->com->time.diff(start_time) < sensor->com->in.get_timeout(sensor->com->user_data)) {
        err = tssPeekHeader(sensor->com, &sensor->header_cfg, &header);
        if(err) {
            continue;
        }

        if(header.echo == cmd_num) {
            if(peekValidatePacket(sensor, &header, min_data_len, max_data_len) == 0) {
                return THREESPACE_AWAIT_COMMAND_FOUND;
            }
            handleMisalignment(sensor);
        }
        else {
            //The data read was not a response to the command being awaited, but
            //may be a response to something else, so call the internal update system to handle
            internalUpdate(sensor, &header);
        }
        
    }

    return THREESPACE_AWAIT_COMMAND_TIMEOUT;
}

static int awaitGetSettingResponse(TSS_Sensor *sensor, uint16_t min_len, bool check_bootloader)
{
    struct TSS_Header header;
    const struct TSS_Setting *setting;
    char buffer[TSS_MAX_SETTINGS_KEY_LEN];
    int num_read_or_err;
    uint32_t id;
    tss_time_t start_time;

    char boot_check[3] = {0};

    //Size of the header
    if(min_len < TSS_BINARY_SETTINGS_ID_SIZE) {
        min_len = TSS_BINARY_SETTINGS_ID_SIZE;
    }

    start_time = sensor->com->time.get();
    while(sensor->com->time.diff(start_time) < sensor->com->in.get_timeout(sensor->com->user_data)) {
        if(sensor->com->in.length(sensor->com->user_data) < min_len) {
            continue;
        }

        if(check_bootloader) {
            sensor->com->in.peek(0, 2, boot_check, sensor->com->user_data);
            if(strcmp(boot_check, "OK") == 0) {
                return THREESPACE_AWAIT_BOOTLOADER;
            }
        }

        //Check for the ID/Echo for getting settings
        tssPeekSettingsHeader(sensor->com, &id);
        if(id != TSS_BINARY_READ_SETTINGS_ID) {
            internalUpdate(sensor, tryPeekHeader(sensor, &header));
            continue;
        }

        //Check to see if the response after the ID looks like a setting
        num_read_or_err = sensor->com->in.peek_until(TSS_BINARY_SETTINGS_ID_SIZE, '\0', buffer, sizeof(buffer), sensor->com->user_data);
        if(num_read_or_err <= 0) {
            //May just not have enough data yet
            continue;
        }

        if(buffer[num_read_or_err-1] != '\0') {
            internalUpdate(sensor, tryPeekHeader(sensor, &header));
            continue;
        }

        setting = tssGetSetting(buffer);
        if(setting == NULL && strcmp(buffer, TSS_SETTING_KEY_ERR_STRING) != 0) {
            internalUpdate(sensor, tryPeekHeader(sensor, &header));
            continue;
        }

        //Good enough, this is more then likely a GetSetting response
        return THREESPACE_AWAIT_COMMAND_FOUND;
    }

    return THREESPACE_AWAIT_COMMAND_TIMEOUT;
}

static int awaitSetSettingResponse(TSS_Sensor *sensor, uint16_t num_keys)
{
    struct TSS_Header header;
    uint8_t buffer[TSS_BINARY_WRITE_SETTING_RESPONSE_LEN], err, num_success, checksum;
    uint32_t id;
    tss_time_t start_time;

    start_time = sensor->com->time.get();
    while(sensor->com->time.diff(start_time) < sensor->com->in.get_timeout(sensor->com->user_data)) {
        if(sensor->com->in.length(sensor->com->user_data) < TSS_BINARY_WRITE_SETTING_WITH_HEADER_RESPONSE_LEN) {
            continue;
        }
        
        //Check for the ID/Echo for getting settings
        tssPeekSettingsHeader(sensor->com, &id);
        if(id != TSS_BINARY_WRITE_SETTINGS_ID) {
            internalUpdate(sensor, tryPeekHeader(sensor, &header));
            continue;
        }

        //Peek the full response and validate the checksum and proper format
        //No need to check the response length because the length was already validated by the first if statement
        sensor->com->in.peek(TSS_BINARY_SETTINGS_ID_SIZE, TSS_BINARY_WRITE_SETTING_RESPONSE_LEN, buffer, sensor->com->user_data);
        err = buffer[0];
        num_success = buffer[1];
        checksum = buffer[2];

        if( (checksum != err + num_success) ||  //Mismatch Checksum
            (err == TSS_SUCCESS && num_success != num_keys) || //Success but not all success?
            (err != TSS_SUCCESS && num_success >= num_keys) || //Not success but all success?
            (num_success > num_keys)) //Invalid num_success value
        {
            internalUpdate(sensor, tryPeekHeader(sensor, &header));
            continue;
        }

        return THREESPACE_AWAIT_COMMAND_FOUND;
    }
    return THREESPACE_AWAIT_COMMAND_TIMEOUT;
}

static int internalUpdate(TSS_Sensor *sensor, const struct TSS_Header *header) {
    uint16_t expected_out_size;
    if(header != NULL) {
        
        if(sensor->streaming.data.active && header->echo == TSS_STREAMING_DATA_BATCH_COMMAND_NUM) {
            expected_out_size = sensor->streaming.data.output_size;
            if(sensor->com->in.length(sensor->com->user_data) < expected_out_size + sensor->header_cfg.size) {
                return THREESPACE_UPDATE_COMMAND_NOT_ENOUGH_DATA;
            }
            if(peekValidatePacket(sensor, header, expected_out_size, expected_out_size) == TSS_SUCCESS) {
                sensorInternalUpdateDataStreaming(sensor);
                return THREESPACE_UPDATE_COMMAND_PARSED;
            }
        }
        else if(sensor->streaming.log.active && header->echo == TSS_STREAMING_FILE_READ_BYTES_COMMAND_NUM) {
            expected_out_size = (header->length < TSS_LOG_STREAMING_MAX_PACKET_SIZE) ? header->length : TSS_LOG_STREAMING_MAX_PACKET_SIZE;
            if(sensor->com->in.length(sensor->com->user_data) < expected_out_size + sensor->header_cfg.size) {
                return THREESPACE_UPDATE_COMMAND_NOT_ENOUGH_DATA;
            }
            if(peekValidatePacket(sensor, header, expected_out_size, expected_out_size) == TSS_SUCCESS) {
                sensorInternalUpdateLogStreaming(sensor);
                return THREESPACE_UPDATE_COMMAND_PARSED;
            }
        }
        else if(sensor->streaming.file.active && header->echo == TSS_STREAMING_FILE_READ_BYTES_COMMAND_NUM) {
            expected_out_size = (header->length < TSS_FILE_STREAMING_MAX_PACKET_SIZE) ? header->length : TSS_FILE_STREAMING_MAX_PACKET_SIZE;
            if(sensor->com->in.length(sensor->com->user_data) < expected_out_size + sensor->header_cfg.size) {
                return THREESPACE_UPDATE_COMMAND_NOT_ENOUGH_DATA;
            }
            if(peekValidatePacket(sensor, header, expected_out_size, expected_out_size) == TSS_SUCCESS) {
                sensorInternalUpdateFileStreaming(sensor);
                return THREESPACE_UPDATE_COMMAND_PARSED;
            }
        }
    }

    handleMisalignment(sensor);
    return THREESPACE_UPDATE_COMMAND_MISALIGNED;
}


#endif /* !TSS_MINIMAL_SENSOR */