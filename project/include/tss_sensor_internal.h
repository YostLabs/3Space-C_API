#ifndef __TSS_SENSOR_INTERNAL_H__
#define __TSS_SENSOR_INTERNAL_H__

#include "tss_sensor.h"

typedef int (*SensorInternalReadFunction)(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs);

//------------------Core Functions------------------------
int sensorInternalExecuteCommand(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, ...);
int sensorInternalExecuteCommandV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, va_list outputs);
int sensorInternalExecuteCommandCustom(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, ...);

//These needs implemented per sensor type
int sensorInternalBaseCommandRead(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs);
int sensorInternalExecuteCommandCustomV(TSS_Sensor *sensor, const struct TSS_Command *command, const void **input, SensorInternalReadFunction read_func, va_list outputs);
int sensorInternalBootloaderCheckActive(TSS_Sensor *sensor, uint8_t *active);

//-----------------------Control-------------------------
void sensorInternalForceStopStreaming(TSS_Sensor *sensor);

//Frequently used helper
static inline void sensorInternalHandleHeader(TSS_Sensor *sensor) {
    if(sensor->_header_enabled) {
        tssReadHeader(sensor->com, &sensor->header_cfg, &sensor->last_header);
    }
}

//-----------------------Custom parsing-----------------------
int sensorInternalProcessStreamingBatch(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs);
int sensorInternalReadStreamingBatch(TSS_Sensor *sensor, const struct TSS_Command *command, va_list outputs);
int sensorInternalReadStreamingBatchChecksumOnly(TSS_Sensor *sensor);
int sensorInternalUpdateDataStreaming(TSS_Sensor *sensor);

int sensorInternalUpdateFileStreaming(TSS_Sensor *sensor);
int sensorInternalUpdateLogStreaming(TSS_Sensor *sensor);

int sensorInternalUpdateDebugMessage(TSS_Sensor *sensor);


#endif /* __TSS_SENSOR_INTERNAL_H__ */