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
    uint8_t in_bootloader;
    sensorInternalForceStopStreaming(sensor);

    //Clear out any garbage data
    sensor->com->in.clear_timeout(sensor->com->user_data, 5);

    //Check for bootloader
    sensorInternalBootloaderCheckActive(sensor, &in_bootloader);
    sensor->_in_bootloader = in_bootloader;
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
    tssGetSettingsWrite(sensor->com, false, key_string);
    result = tssGetSettingsReadV(sensor->com, outputs);

    //Failed to read, likely left over unparsed data. Clear it out to attempt to recover
    if(result < 0) {
        sensor->com->in.clear_immediate(sensor->com->user_data);
    }
    return result;
}

int sensorReadSettingsQuery(TSS_Sensor *sensor, const char *key_string, TssGetSettingsCallback cb, void *user_data)
{
    int result;
    tssGetSettingsWrite(sensor->com, false, key_string);
    result = tssGetSettingsReadCb(sensor->com, cb, user_data);

    //Failed to read, likely left over unparsed data. Clear it out to attempt to recover
    if(result < 0) {
        sensor->com->in.clear_immediate(sensor->com->user_data);
    }    
    return result;
}

int sensorWriteSettings(TSS_Sensor *sensor, const char **keys, uint8_t num_keys, 
    const void **data)
{
    tssSetSettingsWrite(sensor->com, false, keys, num_keys, data);
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

//--------------------------------------BOOTLOADER----------------------------------------------
int sensorInternalBootloaderCheckActive(TSS_Sensor *sensor, uint8_t *active)
{
    char response[2];
    int result;

    //This first part primes the potential Automatic Uart Baudrate detection the bootloader does
    TSS_COM_BEGIN_WRITE(sensor->com);
    sensor->com->out.write((uint8_t*)"UUU", 3, sensor->com->user_data);
    TSS_COM_END_WRITE(sensor->com);
    //Sending the ?'s as a setting causes an instant error response in firmware mode, which prevents
    //having to wait for the bootloader response to timeout to know if in bootloader or firmware
    tssGetSettingsWrite(sensor->com, true, "?");

    result = sensor->com->in.read(2, (uint8_t*)response, sensor->com->user_data);
    if(result != 2) {
        result = TSS_ERR_READ;
    }
    //In bootloader response
    else if(response[0] == 'O' && response[1] == 'K') {
        *active = 1;
        result = TSS_SUCCESS;
    }
    //Start of the "<KEY_ERROR>" response because in firmware
    else if(response[0] == '<' && response[1] == 'K') {
        *active = 0;
        result = TSS_SUCCESS;
    }
    else {
        result = TSS_ERR_UNEXPECTED_CHARACTER;
    }

    //Clear out the rest of the OK responses or rest of the Setting response
    sensor->com->in.clear_immediate(sensor->com->user_data);

    return result;
}

int sensorBootloaderIsActive(TSS_Sensor *sensor, uint8_t *active)
{
    return sensorInternalBootloaderCheckActive(sensor, active);
}

#endif /* TSS_MINIMAL_SENSOR */
