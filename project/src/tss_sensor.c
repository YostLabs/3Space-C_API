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
    printf("Updating stream slots.\n");
    sensorReadStreamSlots(sensor, stream_slots, sizeof(stream_slots));
    tssUtilStreamSlotStringToCommands(stream_slots, sensor->streaming.data.commands);
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

int sensorInternalExecuteCommandCustomV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, va_list outputs)
{
    int err_or_checksum;
    checkDirty(sensor);
    tssWriteCommand(sensor->com, sensor->_header_enabled, command, input);
    if(sensor->_header_enabled) {
        tssReadHeader(sensor->com, &sensor->header_cfg, &sensor->last_header);
    }
    err_or_checksum = read_func(sensor, command, outputs);
    if(err_or_checksum < 0) return err_or_checksum; //Return the error
    return TSS_SUCCESS; //No error
}

//--------------------------------GENERIC FUNCTIONS-------------------------------------

int sensorReadSettingsV(TSS_Sensor *sensor, const char *key_string, va_list outputs)
{
    int result;
    checkDirty(sensor);
    tssGetSettingsWrite(sensor->com, key_string);
    result = tssGetSettingsReadV(sensor->com, outputs);

    //Failed to read, likely left over unparsed data. Clear it out to attempt to recover
    if(result < 0) {
        sensor->com->in.clear_immediate(sensor->com->user_data);
    }
    return result;
}

int sensorReadSettingsQuery(TSS_Sensor *sensor, const char *key_string, TssGetSettingsCallback cb, void *user_data)
{
    checkDirty(sensor);
    tssGetSettingsWrite(sensor->com, key_string);
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
    uint16_t i;

    checkDirty(sensor);

    //Must check for debug_mode=1 before sending the change to be able to properly handle
    //reading the response (since debug messages may be output immediately before the response happens)
    checkAndCacheDebugMode(sensor, keys, num_keys, data);

    tssSetSettingsWrite(sensor->com, keys, num_keys, data);
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


    return result;
}

int sensorUpdateStreaming(TSS_Sensor *sensor)
{
    checkDirty(sensor);
    if(sensor->streaming.data.active) {
        sensorInternalUpdateDataStreaming(sensor);
    }
    if(sensor->streaming.file.active) {
        sensorInternalUpdateFileStreaming(sensor);
    }  
    if(sensor->streaming.log.active) {
        sensorInternalUpdateLogStreaming(sensor);
    }
}

#endif /* !TSS_MINIMAL_SENSOR */