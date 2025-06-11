#include "tss_config.h"
#if TSS_MINIMAL_SENSOR
#include "tss_sensor.h"
#include "tss_sensor_internal.h"
#include "tss_api.h"
#include "tss_string.h"
#include "tss_util.h"
#include "tss_errors.h"

void createTssSensor(TSS_Sensor *sensor, struct TSS_Com_Class *com)
{
    *sensor = (TSS_Sensor) {
        .com = com
    };
}

//----------------------------------CORE SENSOR FUNCTIONS------------------------------------------------------

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
    sensorUpdateCachedSettings(sensor);
}

void sensorUpdateCachedSettings(TSS_Sensor *sensor) {
    char stream_slots[130];
    uint8_t value;

    //Warn if debug mode 1 is enabled
    sensorReadDebugMode(sensor, &value);
    if(value == 1) {
        printf("WARNING: Immediate debug mode enabled, may corrupt data.\n");
    }
    
    //Cache the header
    sensorReadHeader(sensor, &value);
    sensor->header_cfg = tssHeaderInfoFromBitfield(value);

    //Cache the streaming batch
    sensorReadStreamSlots(sensor, stream_slots, sizeof(stream_slots));
    tssUtilStreamSlotStringToCommands(stream_slots, sensor->streaming.data.commands);
}

int sensorInternalBaseCommandRead(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs)
{
    sensorInternalHandleHeader(sensor);
    return tssReadCommandV(sensor->com, command, outputs);
}

int sensorInternalProcessStreamingBatch(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs)
{
    sensorInternalHandleHeader(sensor);
    return sensorInternalReadStreamingBatch(sensor, command, outputs);
}

int sensorInternalExecuteCommandCustomV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, va_list outputs)
{
    int err_or_checksum;
    tssWriteCommand(sensor->com, sensor->_header_enabled, command, input);
    err_or_checksum = read_func(sensor, command, outputs);
    if(err_or_checksum < 0) return err_or_checksum; //Return the error
    return TSS_SUCCESS; //No error
}

//--------------------------------GENERIC FUNCTIONS-------------------------------------

int sensorReadSettingsV(TSS_Sensor *sensor, const char *key_string, va_list outputs)
{
    int result;
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
    tssGetSettingsWrite(sensor->com, key_string);
    return tssGetSettingsReadCb(sensor->com, cb, user_data);
}

int sensorWriteSettings(TSS_Sensor *sensor, const char **keys, uint8_t num_keys, 
    const void **data)
{
    tssSetSettingsWrite(sensor->com, keys, num_keys, data);
    return tssSetSettingsRead(sensor->com, &sensor->last_write_setting_response);
}

int sensorUpdateStreaming(TSS_Sensor *sensor)
{
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

#endif /* TSS_MINIMAL_SENSOR */