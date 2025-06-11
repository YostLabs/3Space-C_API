/*
*  Common to all sensor types
*/

#include "tss_sensor.h"
#include "tss_sensor_internal.h"
#include "tss_constants.h"
#include "tss_config.h"
#include "tss_errors.h"
#include <stdarg.h>

//----------------------------USER FACING--------------------------------

int sensorReadSettings(TSS_Sensor *sensor, const char *key_string, ...) {
    va_list args;
    int result;

    va_start(args, key_string);
    result = sensorReadSettingsV(sensor, key_string, args);
    va_end(args);
    return result;
}

int sensorProcessDataStreamingCallbackOutput(TSS_Sensor *sensor, ...)
{
    va_list outputs;
    int result;

    va_start(outputs, sensor);
    result = sensorInternalReadStreamingBatch(sensor, NULL, outputs);
    va_end(outputs);

    return result;
}

int sensorProcessFileStreamingCallbackOutput(TSS_Sensor *sensor, void *output, uint16_t size)
{
    uint16_t num_read, read_len;
    read_len = sensor->streaming.file.remaining_cur_packet_len;
    if(read_len == 0) {
        return 0;
    }
    if(read_len > size) {
        read_len = size;
    }
    num_read = sensor->com->in.read(read_len, output, sensor->com->user_data);
    sensor->streaming.file.remaining_cur_packet_len -= num_read;
    if(num_read != read_len) {
        return TSS_ERR_READ;
    }
    return num_read;
}


#include <stdio.h>

//--------------------------INTERNAL--------------------------------------
void sensorInternalForceStopStreaming(TSS_Sensor *sensor)
{   
    bool header_was_enabled, was_dirty;
    header_was_enabled = sensor->_header_enabled;
    was_dirty = sensor->dirty;
    sensor->_header_enabled = false;
    sensor->dirty = false;

    //These functions return no data, so can be called regardless of if available.
    //This affectively sends these and reads nothing by having header disabled for these
    sensorStopStreaming(sensor);
    sensorStopStreamingFile(sensor);
    sensorStopLogging(sensor);

    sensor->_header_enabled = header_was_enabled;
    sensor->dirty = was_dirty;
}

//---------------------------------------STREAMING FUNCTIONALITY-------------------------------------------------

int sensorInternalReadStreamingBatch(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs) {
    uint8_t checksum;
    int err_or_checksum;
    const struct TSS_Command **cur_slot;
    
    checksum = 0;
    cur_slot = sensor->streaming.data.commands;
    while(*cur_slot != NULL) {
        err_or_checksum = tssReadCommandVp(sensor->com, *cur_slot, &outputs);
        if(err_or_checksum < 0) {
            return err_or_checksum;
        }
        checksum += err_or_checksum;
        cur_slot++;
    }

    return checksum;
}

int sensorInternalReadStreamingBatchChecksumOnly(TSS_Sensor *sensor) {
    uint8_t checksum;
    int err_or_checksum;
    const struct TSS_Command **cur_slot;

    checksum = 0;
    cur_slot = sensor->streaming.data.commands;
    while(*cur_slot != NULL) {
        err_or_checksum = tssReadCommandChecksumOnly(sensor->com, *cur_slot);
        if(err_or_checksum < 0) {
            return err_or_checksum;
        }
        checksum += err_or_checksum;
        cur_slot++;
    }

    return checksum;
}

int sensorInternalUpdateDataStreaming(TSS_Sensor *sensor) {
    sensorInternalHandleHeader(sensor);
    enum TSS_StreamingCallbackState state = sensor->streaming.data.cb(sensor);
    if(state == TSS_StreamingCallbackStateIgnored) {
        sensorInternalReadStreamingBatchChecksumOnly(sensor);
    }
    return state;
}

int sensorInternalUpdateFileStreaming(TSS_Sensor *sensor)
{
    uint16_t packet_len;

    sensorInternalHandleHeader(sensor);

#if TSS_MINIMAL_SENSOR
    if(sensor->streaming.file.remaining_len > TSS_FILE_STREAMING_MAX_PACKET_SIZE) {
        packet_len = TSS_FILE_STREAMING_MAX_PACKET_SIZE;
    }
    else {
        packet_len = sensor->streaming.file.remaining_len;
    }
#else
    packet_len = sensor->last_header.length;
#endif
    sensor->streaming.file.remaining_cur_packet_len = packet_len;

    enum TSS_StreamingCallbackState state = sensor->streaming.file.cb(sensor);
    if(sensor->streaming.file.remaining_cur_packet_len > 0) { //Read unread data out
        tssReadBytesChecksumOnly(sensor->com, sensor->streaming.file.remaining_cur_packet_len, NULL);
    }

    sensor->streaming.file.remaining_len -= packet_len;
    if(sensor->streaming.file.remaining_len == 0) {
        sensor->streaming.file.active = false;
    }

    return state;
}

int sensorInternalUpdateLogStreaming(TSS_Sensor *sensor) {
    if(sensor->streaming.log.header_enabled) {
        tssReadHeader(sensor->com, &sensor->header_cfg, &sensor->last_header);
    }
    enum TSS_StreamingCallbackState state = sensor->streaming.log.cb(sensor);
    if(state == TSS_StreamingCallbackStateIgnored) {
#if TSS_MINIMAL_SENSOR
        sensor->com->in.clear_immediate(sensor->com);
#else
        tssReadBytesChecksumOnly(sensor->com, sensor->last_header.length, NULL);
#endif
    }
    return state;
}

//------------------------WRAPPERS---------------------------------
int sensorInternalExecuteCommand(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, ...)
{
    va_list args;
    int result;

    va_start(args, input);
    result = sensorInternalExecuteCommandV(sensor, command, input, args);
    va_end(args);
    return result;
}

int sensorInternalExecuteCommandV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, va_list outputs)
{
    return sensorInternalExecuteCommandCustomV(sensor, command, input, sensorInternalBaseCommandRead, outputs);
}

int sensorInternalExecuteCommandCustom(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, ...)
{
    va_list args;
    int result;

    va_start(args, read_func);
    result = sensorInternalExecuteCommandCustomV(sensor, command, input, read_func, args);
    va_end(args);
    return result;
}