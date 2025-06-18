 /**
 * @ Author: Andy Riedlinger
 * @ Create Time: 2025-5-22 1:01:00
 *
 * @ Description:
 */
#ifndef __TSS_SENSOR_H__
#define __TSS_SENSOR_H__

#include "tss/com/com_class.h"
#include "tss/api/header.h"
#include "tss/api/command.h"
#include "tss/api/core.h"
#include <stdbool.h>

//-----------------------------TYPE DECLARATIONS-----------------------------------

enum TSS_DataCallbackState {
    TSS_DataCallbackStateError = -1,
    TSS_DataCallbackStateIgnored = 0,
    TSS_DataCallbackStateProcessed = 1
};

typedef struct TSS_Sensor TSS_Sensor;
typedef enum TSS_DataCallbackState (*TssDataCallback)(TSS_Sensor *sensor);

struct TSS_Sensor {
    struct TSS_Com_Class *com;

    //Config Info
    bool _header_enabled;
    struct TSS_Header_Info header_cfg;

    struct {
        TssDataCallback cb;
        uint16_t bytes_read;
        bool _immediate;
        bool message_processed;
    } debug;

    //Cached Data
    struct TSS_Header last_header;
    struct TSS_Setting_Response last_write_setting_response;

    //Streaming Information
    struct {
        struct {
            TssDataCallback cb;
            const struct TSS_Command* commands[17];
            uint16_t output_size;
            bool active;
        } data;
        struct {
            TssDataCallback cb;
            uint64_t remaining_len;
            uint16_t remaining_cur_packet_len;
            bool active;
        } file;
        struct {
            TssDataCallback cb;
            uint8_t header_enabled;
            bool active;
        } log;
    } streaming;

    //Control/Status Info
    bool _in_bootloader;
    bool dirty; //Unknown setting state. Cached values may be incorrect.

    //Cached Data (Either useful for user or required for some functionality)
    uint64_t serial_number;

    void *user_data;
};

//------------------------INITIALIZATION----------------------------------------
void createTssSensor(TSS_Sensor *sensor, struct TSS_Com_Class *com);
void initTssSensor(TSS_Sensor *sensor);

//-------------------------------MANUAL MANAGEMENT-----------------------------------------
void sensorUpdateCachedSettings(TSS_Sensor *sensor);

/// @brief Should be called if changes are made to the sensor without using the sensor API.
/// The sensor will automatically reinitialize to a known state next time an API function is called
/// via the sensorUpdateCachedSettings function.
/// @note This does not work with the Minimal API. If using the Minimal API, you must call sensorUpdateCachedSettings
/// manually.
/// @param sensor The sensor that had modifications made
static inline void sensorMarkSettingsDirty(TSS_Sensor *sensor) {
    sensor->dirty = true;
}

//----------------------------SETTERS--------------------------------------
static inline void sensorSetDebugCallback(TSS_Sensor *sensor, TssDataCallback callback) {
    sensor->debug.cb = callback;
}
static inline void sensorSetUserData(TSS_Sensor *sensor, void *user_data) {
    sensor->user_data = user_data;
}

//----------------------------GETTERS--------------------------------------
static inline struct TSS_Header sensorGetLastHeader(TSS_Sensor *sensor) {
    return sensor->last_header;
}
static inline struct TSS_Setting_Response sensorGetLastSettingResponse(TSS_Sensor *sensor) {
    return sensor->last_write_setting_response;
}
static inline bool sensorIsStreaming(TSS_Sensor *sensor) {
    return sensor->streaming.data.active || sensor->streaming.file.active || sensor->streaming.log.active;
}
static inline void* sensorGetUserData(TSS_Sensor *sensor) {
    return sensor->user_data;
}

//--------------------------------BASE FUNCTIONALITY-----------------------------------------
int sensorReadSettings(TSS_Sensor *sensor, const char *key_string, ...);
int sensorReadSettingsV(TSS_Sensor *sensor, const char *key_string, va_list outputs);
int sensorReadSettingsQuery(TSS_Sensor *sensor, const char *key_string, TssGetSettingsCallback cb, void *user_data);
int sensorWriteSettings(TSS_Sensor *sensor, const char **keys, uint8_t num_keys, const void **data);

int sensorUpdateStreaming(TSS_Sensor *sensor);
int sensorProcessDataStreamingCallbackOutput(TSS_Sensor *sensor, ...);
/// @brief Reads file streaming data inside the file streaming callback
/// @param sensor The sensor object
/// @param output Where to read the data to
/// @param size The size of output
/// @return The number of bytes read, or negative on error.
/// @note No data left when return value < size
int sensorProcessFileStreamingCallbackOutput(TSS_Sensor *sensor, void *output, uint16_t size);

int sensorProcessDebugCallbackOutput(TSS_Sensor *sensor, char *output, size_t size);

//--------------------------------CUSTOM COMMAND DECLARATIONS--------------------------------------
int sensorStartStreaming(TSS_Sensor *sensor, TssDataCallback cb);
int sensorStreamFile(TSS_Sensor *sensor, TssDataCallback cb, uint64_t *out_size);
int sensorStartLogging(TSS_Sensor *sensor, TssDataCallback cb);

/// @brief Disconnects and reconnects to the sensor. May
/// require additional com class functionality (Reenumerate/Auto Detect)
/// @param sensor The sensor to connect to
/// @param timeout_ms How long to attempt to reconnect
/// @return TSS_SUCCESS if successfully connected
/// @warning On failure, the provided sensor object is in an undefined state
int sensorReconnect(TSS_Sensor *sensor, uint32_t timeout_ms);
int sensorCleanup(TSS_Sensor *sensor);

//---------------------------------BOOTLOADER COMMANDS-------------------------------------------
struct TSS_Bootloader_Info {
    int32_t memstart;
    int32_t memend;
    int16_t pagesize;
    int16_t version; 
};

int sensorBootloaderIsActive(TSS_Sensor *sensor, uint8_t *active);
int sensorBootloaderGetSerialNumber(TSS_Sensor *sensor, uint64_t *serial_number);
int sensorBootloaderLoadFirmware(TSS_Sensor *sensor, uint32_t timeout_ms);
int sensorBootloaderEraseFirmware(TSS_Sensor *sensor, uint32_t timeout_ms);
int sensorBootloaderGetInfo(TSS_Sensor *sensor, struct TSS_Bootloader_Info *info);
int sensorBootloaderProgram(TSS_Sensor *sensor, uint8_t *bytes, uint16_t num_bytes, uint32_t timeout_ms);
int sensorBootloaderGetStatus(TSS_Sensor *sensor, uint32_t *status);
int sensorBootloaderRestoreFactorySettings(TSS_Sensor *sensor);


//-------------------------------AUTO GENERATED COMMANDS--------------------------------------------
int sensorGetTaredOrientation(TSS_Sensor *sensor, float out_quat[4]);
int sensorGetTaredOrientationAsEulerAngles(TSS_Sensor *sensor, float out_euler[3]);
int sensorGetTaredOrientationAsRotationMatrix(TSS_Sensor *sensor, float out_matrix[9]);
int sensorGetTaredOrientationAsAxisAngles(TSS_Sensor *sensor, float out_axis[3], float *out_angle);
int sensorGetTaredOrientationAsTwoVector(TSS_Sensor *sensor, float out_forward[3], float out_down[3]);
int sensorGetDifferenceQuaternion(TSS_Sensor *sensor, float out_quat[4]);
int sensorGetUntaredOrientation(TSS_Sensor *sensor, float out_quat[4]);
int sensorGetUntaredOrientationAsEulerAngles(TSS_Sensor *sensor, float out_euler[3]);
int sensorGetUntaredOrientationAsRotationMatrix(TSS_Sensor *sensor, float out_matrix[9]);
int sensorGetUntaredOrientationAsAxisAngles(TSS_Sensor *sensor, float out_axis[3], float *out_angle);
int sensorGetUntaredOrientationAsTwoVector(TSS_Sensor *sensor, float out_forward[3], float out_down[3]);
int sensorGetTaredTwoVectorInSensorFrame(TSS_Sensor *sensor, float out_north[3], float out_down[3]);
int sensorGetUntaredTwoVectorInSensorFrame(TSS_Sensor *sensor, float out_north[3], float out_down[3]);
int sensorGetPrimaryBarometerPressure(TSS_Sensor *sensor, float *out_mbar);
int sensorGetPrimaryBarometerAltitude(TSS_Sensor *sensor, float *out_meters);
int sensorGetBarometerAltitudeByID(TSS_Sensor *sensor, uint8_t id, float *out_meters);
int sensorGetBarometerPressureByID(TSS_Sensor *sensor, uint8_t id, float *out_mbar);
int sensorSetOffsetWithCurrentOrientation(TSS_Sensor *sensor);
int sensorResetBaseOffset(TSS_Sensor *sensor);
int sensorSetBaseOffsetWithCurrentOrientation(TSS_Sensor *sensor);
int sensorGetInterruptStatus(TSS_Sensor *sensor, uint8_t *out_status);
int sensorGetAllNormalizedComponentSensorData(TSS_Sensor *sensor, float out_gyro[3], float out_accel[3], float out_mag[3]);
int sensorGetNormalizedGyroRateVector(TSS_Sensor *sensor, float out_gyro[3]);
int sensorGetNormalizedAccelerometerVector(TSS_Sensor *sensor, float out_accel[3]);
int sensorGetNormalizedMagnetometerVector(TSS_Sensor *sensor, float out_mag[3]);
int sensorGetAllCorrectedComponentSensorData(TSS_Sensor *sensor, float out_gyro[3], float out_accel[3], float out_mag[3]);
int sensorGetCorrectedGyroRateVector(TSS_Sensor *sensor, float out_gyro[3]);
int sensorGetCorrectedAccelerometerVector(TSS_Sensor *sensor, float out_accel[3]);
int sensorGetCorrectedMagnetometerVector(TSS_Sensor *sensor, float out_mag[3]);
int sensorGetCorrectedGlobalLinearAcceleration(TSS_Sensor *sensor, float out_accel[3]);
int sensorGetCorrectedLocalLinearAcceleration(TSS_Sensor *sensor, float out_accel[3]);
int sensorGetTemperatureCelsius(TSS_Sensor *sensor, float *out_celsius);
int sensorGetTemperatureFahrenheit(TSS_Sensor *sensor, float *out_fahrenheit);
int sensorGetMotionlessConfidenceFactor(TSS_Sensor *sensor, float *out_confidence);
int sensorCorrectRawGyroRateVector(TSS_Sensor *sensor, const float raw_gyro[3], uint8_t id, float out_corrected[3]);
int sensorCorrectRawAccelerometerVector(TSS_Sensor *sensor, const float raw_accel[3], uint8_t id, float out_corrected[3]);
int sensorCorrectRawMagnetometerVector(TSS_Sensor *sensor, const float raw_mag[3], uint8_t id, float out_corrected[3]);
int sensorGetNormalizedGyroRateByID(TSS_Sensor *sensor, uint8_t id, float out_gyro[3]);
int sensorGetNormalizedAccelerometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_accel[3]);
int sensorGetNormalizedMagnetometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_mag[3]);
int sensorGetCorrectedGyroRateByID(TSS_Sensor *sensor, uint8_t id, float out_gyro[3]);
int sensorGetCorrectedAccelerometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_accel[3]);
int sensorGetCorrectedMagnetometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_mag[3]);
int sensorEnableMassStorageController(TSS_Sensor *sensor);
int sensorDisableMassStorageController(TSS_Sensor *sensor);
int sensorFormatSDCard(TSS_Sensor *sensor);
int sensorStopLogging(TSS_Sensor *sensor);
int sensorSetClockValues(TSS_Sensor *sensor, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int sensorGetClockValues(TSS_Sensor *sensor, uint16_t *out_year, uint8_t *out_month, uint8_t *out_day, uint8_t *out_hour, uint8_t *out_minute, uint8_t *out_second);
int sensorGetLoggingStatus(TSS_Sensor *sensor, uint8_t *out_status);
int sensorGetRawGyroRateByID(TSS_Sensor *sensor, uint8_t id, float out_gyro[3]);
int sensorGetRawAccelerometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_accel[3]);
int sensorGetRawMagnetometerVectorByID(TSS_Sensor *sensor, uint8_t id, float out_mag[3]);
int sensorEEPTSStart(TSS_Sensor *sensor);
int sensorEEPTSStop(TSS_Sensor *sensor);

/// @brief Retrieves the oldest EEPTS output struct
/// @param sensor 
/// @return 
/// @note Use this with a struct TSS_EEPTS_Output and the TSS_EEPTS_OUT(&struct) macro
/// This is done instead of directly taking a struct to be consistent with how an EEPTS struct
/// is obtained when retrieving via the Data Streaming callback.
int sensorEEPTSGetOldestStep(TSS_Sensor *sensor, uint32_t *out_segment_count, uint32_t *out_timestamp, double *out_longitude, double *out_latitude, float *out_altitude, float *out_heading, float *out_distance, float *out_distance_x, float *out_distance_y, float *out_distance_z, uint8_t *out_motion, uint8_t *out_location, float *out_confidence, float *out_overall_confidence);
/// @brief Retrieves the newest EEPTS output struct
/// @param sensor 
/// @return 
/// @note Use this with a struct TSS_EEPTS_Output and the TSS_EEPTS_OUT(&struct) macro
/// This is done instead of directly taking a struct to be consistent with how an EEPTS struct
/// is obtained when retrieving via the Data Streaming callback.
int sensorEEPTSGetNewestStep(TSS_Sensor *sensor, uint32_t *out_segment_count, uint32_t *out_timestamp, double *out_longitude, double *out_latitude, float *out_altitude, float *out_heading, float *out_distance, float *out_distance_x, float *out_distance_y, float *out_distance_z, uint8_t *out_motion, uint8_t *out_location, float *out_confidence, float *out_overall_confidence);
int sensorEEPTSGetAvailableStepCount(TSS_Sensor *sensor, uint8_t *out_count);
int sensorEEPTSInsertGPS(TSS_Sensor *sensor, double Latitude, double Longitude);
int sensorEEPTSAutoOffset(TSS_Sensor *sensor);
int sensorGetStreamingCommandLabel(TSS_Sensor *sensor, uint8_t cmd_number, char *out_label, uint32_t size);
int sensorGetStreamingBatch(TSS_Sensor *sensor, ...);
int sensorStopStreaming(TSS_Sensor *sensor);
int sensorPauseLogStreaming(TSS_Sensor *sensor, uint8_t pause);
int sensorGetTimestamp(TSS_Sensor *sensor, uint64_t *out_timestamp);
int sensorSetTimestamp(TSS_Sensor *sensor, uint64_t timestamp);
int sensorSetTareWithCurrentOrientation(TSS_Sensor *sensor);
int sensorSetBaseTareWithCurrentOrientation(TSS_Sensor *sensor);
int sensorResetFilter(TSS_Sensor *sensor);
int sensorGetDebugMessageCount(TSS_Sensor *sensor, uint16_t *out_count);
int sensorGetDebugMessage(TSS_Sensor *sensor, char *out_msg, uint32_t size);
int sensorSelfTest(TSS_Sensor *sensor, uint32_t *out_result);
int sensorBeginPassiveCalibration(TSS_Sensor *sensor, uint8_t mode);
int sensorGetPassiveCalibrationActive(TSS_Sensor *sensor, uint8_t *out_state);
int sensorBeginActiveCalibration(TSS_Sensor *sensor);
int sensorGetActiveCalibrationActive(TSS_Sensor *sensor, uint8_t *out_state);
int sensorGetLastLiveLoggingLocation(TSS_Sensor *sensor, uint64_t *out_cursor_index, char *out_path, uint32_t size);
int sensorGetNextDirectoryItem(TSS_Sensor *sensor, uint8_t *out_type, char *out_name, uint32_t size, uint64_t *out_size);
int sensorChangeDirectory(TSS_Sensor *sensor, const char *path);
int sensorOpenFile(TSS_Sensor *sensor, const char *path);
int sensorCloseFile(TSS_Sensor *sensor);
int sensorGetRemainingFileSize(TSS_Sensor *sensor, uint64_t *out_size);
int sensorReadLine(TSS_Sensor *sensor, char *out_line, uint32_t size);
int sensorReadBytes(TSS_Sensor *sensor, uint16_t len, uint8_t *out_data);
int sensorDeleteFileOrFolder(TSS_Sensor *sensor, const char *path);
int sensorSetCursorIndex(TSS_Sensor *sensor, uint64_t index);
int sensorStopStreamingFile(TSS_Sensor *sensor);
int sensorGetBatteryVoltage(TSS_Sensor *sensor, float *out_voltage);
int sensorGetBatteryPercent(TSS_Sensor *sensor, uint8_t *out_percent);
int sensorGetBatteryStatus(TSS_Sensor *sensor, uint8_t *out_status);
int sensorGetGPSLatitudeandLongitude(TSS_Sensor *sensor, double *out_latitude, double *out_longitude);
int sensorGetGPSAltitude(TSS_Sensor *sensor, float *out_meters);
int sensorGetGPSFixStatus(TSS_Sensor *sensor, uint8_t *out_fix);
int sensorGetGPSHDOP(TSS_Sensor *sensor, uint8_t *out_hdop);
int sensorGetGPSSatellites(TSS_Sensor *sensor, uint8_t *out_num_satellites);
int sensorCommitSettings(TSS_Sensor *sensor);
int sensorSoftwareReset(TSS_Sensor *sensor);
int sensorEnterBootloader(TSS_Sensor *sensor);
int sensorGetButtonState(TSS_Sensor *sensor, uint8_t *out_state);

//-------------------------------------MANUALLY DECLARED SETTINGS----------------------------------

int sensorReadCat(TSS_Sensor *sensor, const char *out, uint32_t size);
int sensorWriteCat(TSS_Sensor *sensor, const char *value);

//--------------------------------------AUTO GENERATED SETTINGS-------------------------------------------

int sensorRestoreDefaultSettings(TSS_Sensor *sensor);
int sensorReadAllSettings(TSS_Sensor *sensor, TssGetSettingsCallback cb, void *user_data);
int sensorReadAllWritableSettings(TSS_Sensor *sensor, TssGetSettingsCallback cb, void *user_data);
int sensorReadSerialNumber(TSS_Sensor *sensor, uint64_t *out);
int sensorWriteTimestamp(TSS_Sensor *sensor, uint64_t value);
int sensorReadTimestamp(TSS_Sensor *sensor, uint64_t *out);
int sensorWriteLedMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadLedMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLedRgb(TSS_Sensor *sensor, const float value[3]);
int sensorReadLedRgb(TSS_Sensor *sensor, float out[3]);
int sensorReadVersionFirmware(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadVersionHardware(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadUpdateRateSensor(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteHeader(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeader(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderStatus(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderStatus(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderTimestamp(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderTimestamp(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderEcho(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderEcho(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderChecksum(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderChecksum(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderSerial(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderSerial(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteHeaderLength(TSS_Sensor *sensor, uint8_t value);
int sensorReadHeaderLength(TSS_Sensor *sensor, uint8_t *out);
int sensorReadValidCommands(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteCpuSpeed(TSS_Sensor *sensor, uint32_t value);
int sensorReadCpuSpeed(TSS_Sensor *sensor, uint32_t *out);
int sensorReadCpuSpeedCur(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePmMode(TSS_Sensor *sensor, uint8_t value);
int sensorWritePmIdleEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadPmIdleEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteStreamSlots(TSS_Sensor *sensor, const char *value);
int sensorReadStreamSlots(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteStreamInterval(TSS_Sensor *sensor, uint64_t value);
int sensorReadStreamInterval(TSS_Sensor *sensor, uint64_t *out);
int sensorWriteStreamHz(TSS_Sensor *sensor, float value);
int sensorReadStreamHz(TSS_Sensor *sensor, float *out);
int sensorWriteStreamDuration(TSS_Sensor *sensor, float value);
int sensorReadStreamDuration(TSS_Sensor *sensor, float *out);
int sensorWriteStreamDelay(TSS_Sensor *sensor, float value);
int sensorReadStreamDelay(TSS_Sensor *sensor, float *out);
int sensorWriteStreamMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadStreamMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteStreamCount(TSS_Sensor *sensor, uint64_t value);
int sensorReadStreamCount(TSS_Sensor *sensor, uint64_t *out);
int sensorReadStreamableCommands(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteDebugLevel(TSS_Sensor *sensor, uint32_t value);
int sensorReadDebugLevel(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteDebugModule(TSS_Sensor *sensor, uint32_t value);
int sensorReadDebugModule(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteDebugMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadDebugMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteDebugLed(TSS_Sensor *sensor, uint8_t value);
int sensorReadDebugLed(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteDebugFault(TSS_Sensor *sensor, uint8_t value);
int sensorReadDebugFault(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteDebugWdt(TSS_Sensor *sensor, uint8_t value);
int sensorReadDebugWdt(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteAxisOrder(TSS_Sensor *sensor, const char *value);
int sensorReadAxisOrder(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteAxisOrderC(TSS_Sensor *sensor, const char *value);
int sensorReadAxisOrderC(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteAxisOffsetEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadAxisOffsetEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteEulerOrder(TSS_Sensor *sensor, const char *value);
int sensorReadEulerOrder(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadUpdateRateFilter(TSS_Sensor *sensor, uint32_t *out);
int sensorReadUpdateRateSms(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteOffset(TSS_Sensor *sensor, const float value[4]);
int sensorReadOffset(TSS_Sensor *sensor, float out[4]);
int sensorWriteBaseOffset(TSS_Sensor *sensor, const float value[4]);
int sensorReadBaseOffset(TSS_Sensor *sensor, float out[4]);
int sensorWriteTareQuat(TSS_Sensor *sensor, const float value[4]);
int sensorReadTareQuat(TSS_Sensor *sensor, float out[4]);
int sensorWriteTareAutoBase(TSS_Sensor *sensor, uint8_t value);
int sensorReadTareAutoBase(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteBaseTare(TSS_Sensor *sensor, const float value[4]);
int sensorReadBaseTare(TSS_Sensor *sensor, float out[4]);
int sensorWriteTareMat(TSS_Sensor *sensor, const float value[9]);
int sensorReadTareMat(TSS_Sensor *sensor, float out[9]);
int sensorWriteRunningAvgOrient(TSS_Sensor *sensor, float value);
int sensorReadRunningAvgOrient(TSS_Sensor *sensor, float *out);
int sensorWriteFilterMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadFilterMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteFilterMrefMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadFilterMrefMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteFilterMref(TSS_Sensor *sensor, const float value[3]);
int sensorReadFilterMref(TSS_Sensor *sensor, float out[3]);
int sensorWriteFilterMrefGps(TSS_Sensor *sensor, const double value[2]);
int sensorWriteFilterMrefDip(TSS_Sensor *sensor, float value);
int sensorReadFilterMrefDip(TSS_Sensor *sensor, float *out);
int sensorReadValidAccels(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadValidGyros(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadValidMags(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadValidBaros(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorReadValidComponents(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePrimaryAccel(TSS_Sensor *sensor, const char *value);
int sensorReadPrimaryAccel(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePrimaryGyro(TSS_Sensor *sensor, const char *value);
int sensorReadPrimaryGyro(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePrimaryMag(TSS_Sensor *sensor, const char *value);
int sensorReadPrimaryMag(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePrimarySensorRfade(TSS_Sensor *sensor, float value);
int sensorReadPrimarySensorRfade(TSS_Sensor *sensor, float *out);
int sensorWriteMagBiasMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadMagBiasMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteOdrAll(TSS_Sensor *sensor, uint32_t value);
int sensorWriteOdrAccelAll(TSS_Sensor *sensor, uint32_t value);
int sensorWriteOdrGyroAll(TSS_Sensor *sensor, uint32_t value);
int sensorWriteOdrMagAll(TSS_Sensor *sensor, uint32_t value);
int sensorWriteOdrBaroAll(TSS_Sensor *sensor, uint32_t value);
int sensorWriteAccelEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadAccelEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteGyroEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadGyroEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteMagEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadMagEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteCalibMatAccel(TSS_Sensor *sensor, uint8_t id, const float value[9]);
int sensorReadCalibMatAccel(TSS_Sensor *sensor, uint8_t id, float out[9]);
int sensorWriteCalibBiasAccel(TSS_Sensor *sensor, uint8_t id, const float value[3]);
int sensorReadCalibBiasAccel(TSS_Sensor *sensor, uint8_t id, float out[3]);
int sensorWriteRangeAccel(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadRangeAccel(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorReadValidRangesAccel(TSS_Sensor *sensor, uint8_t id, char *out, uint32_t size);
int sensorWriteOversampleAccel(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadOversampleAccel(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorWriteRunningAvgAccel(TSS_Sensor *sensor, uint8_t id, float value);
int sensorReadRunningAvgAccel(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteOdrAccel(TSS_Sensor *sensor, uint8_t id, uint32_t value);
int sensorReadOdrAccel(TSS_Sensor *sensor, uint8_t id, uint32_t *out);
int sensorReadUpdateRateAccel(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteCalibMatGyro(TSS_Sensor *sensor, uint8_t id, const float value[9]);
int sensorReadCalibMatGyro(TSS_Sensor *sensor, uint8_t id, float out[9]);
int sensorWriteCalibBiasGyro(TSS_Sensor *sensor, uint8_t id, const float value[3]);
int sensorReadCalibBiasGyro(TSS_Sensor *sensor, uint8_t id, float out[3]);
int sensorWriteRangeGyro(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadRangeGyro(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorReadValidRangesGyro(TSS_Sensor *sensor, uint8_t id, char *out, uint32_t size);
int sensorWriteOversampleGyro(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadOversampleGyro(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorWriteRunningAvgGyro(TSS_Sensor *sensor, uint8_t id, float value);
int sensorReadRunningAvgGyro(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteOdrGyro(TSS_Sensor *sensor, uint8_t id, uint32_t value);
int sensorReadOdrGyro(TSS_Sensor *sensor, uint8_t id, uint32_t *out);
int sensorReadUpdateRateGyro(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteCalibMatMag(TSS_Sensor *sensor, uint8_t id, const float value[9]);
int sensorReadCalibMatMag(TSS_Sensor *sensor, uint8_t id, float out[9]);
int sensorWriteCalibBiasMag(TSS_Sensor *sensor, uint8_t id, const float value[3]);
int sensorReadCalibBiasMag(TSS_Sensor *sensor, uint8_t id, float out[3]);
int sensorWriteRangeMag(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadRangeMag(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorReadValidRangesMag(TSS_Sensor *sensor, uint8_t id, char *out, uint32_t size);
int sensorWriteOversampleMag(TSS_Sensor *sensor, uint8_t id, uint16_t value);
int sensorReadOversampleMag(TSS_Sensor *sensor, uint8_t id, uint16_t *out);
int sensorWriteRunningAvgMag(TSS_Sensor *sensor, uint8_t id, float value);
int sensorReadRunningAvgMag(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteOdrMag(TSS_Sensor *sensor, uint8_t id, uint32_t value);
int sensorReadOdrMag(TSS_Sensor *sensor, uint8_t id, uint32_t *out);
int sensorReadUpdateRateMag(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteCalibBiasBaro(TSS_Sensor *sensor, uint8_t id, float value);
int sensorReadCalibBiasBaro(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWriteCalibAltitudeBaro(TSS_Sensor *sensor, uint8_t id, float value);
int sensorWriteOdrBaro(TSS_Sensor *sensor, uint8_t id, uint32_t value);
int sensorReadOdrBaro(TSS_Sensor *sensor, uint8_t id, uint32_t *out);
int sensorReadUpdateRateBaro(TSS_Sensor *sensor, uint8_t id, float *out);
int sensorWritePtsOffsetQuat(TSS_Sensor *sensor, const float value[4]);
int sensorReadPtsOffsetQuat(TSS_Sensor *sensor, float out[4]);
int sensorRestorePtsDefaultSettings(TSS_Sensor *sensor);
int sensorReadPtsSettings(TSS_Sensor *sensor, TssGetSettingsCallback cb, void *user_data);
int sensorWritePtsPresetHand(TSS_Sensor *sensor, uint8_t value);
int sensorWritePtsPresetMotion(TSS_Sensor *sensor, uint8_t value);
int sensorWritePtsPresetHeading(TSS_Sensor *sensor, uint8_t value);
int sensorWritePtsDebugLevel(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsDebugLevel(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsDebugModule(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsDebugModule(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsHeadingMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsHeadingMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsInitialHeadingMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsInitialHeadingMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsHandHeadingMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsHandHeadingMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsMagDeclination(TSS_Sensor *sensor, float value);
int sensorReadPtsMagDeclination(TSS_Sensor *sensor, float *out);
int sensorWritePtsAutoDeclination(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsAutoDeclination(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsDiscardSlow(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsDiscardSlow(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsSegmentAxis(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsSegmentAxis(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsSegNoise(TSS_Sensor *sensor, float value);
int sensorReadPtsSegNoise(TSS_Sensor *sensor, float *out);
int sensorWritePtsClassifierMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsClassifierMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsClassifierMode2(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsClassifierMode2(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsLocationClassifierMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsLocationClassifierMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsHandClassifierThreshold(TSS_Sensor *sensor, float value);
int sensorReadPtsHandClassifierThreshold(TSS_Sensor *sensor, float *out);
int sensorWritePtsDisabledTruthMotions(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsDisabledTruthMotions(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsDynamicSegmenterEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsDynamicSegmenterEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsEstimatorScalars(TSS_Sensor *sensor, const float value[7]);
int sensorReadPtsEstimatorScalars(TSS_Sensor *sensor, float out[7]);
int sensorWritePtsAutoEstimatorScalarRate(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsAutoEstimatorScalarRate(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsRunningCorrection(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsRunningCorrection(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsHandCorrection(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsHandCorrection(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsHeadingCorrectionMode(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsHeadingCorrectionMode(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsHeadingMinDif(TSS_Sensor *sensor, float value);
int sensorReadPtsHeadingMinDif(TSS_Sensor *sensor, float *out);
int sensorWritePtsHeadingResetConsistencies(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsHeadingResetConsistencies(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsHeadingBacktrackEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsHeadingBacktrackEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsMotionCorrectionRadius(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsMotionCorrectionRadius(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsMotionCorrectionConsistencyReq(TSS_Sensor *sensor, uint32_t value);
int sensorReadPtsMotionCorrectionConsistencyReq(TSS_Sensor *sensor, uint32_t *out);
int sensorWritePtsOrientRefYThreshold(TSS_Sensor *sensor, float value);
int sensorReadPtsOrientRefYThreshold(TSS_Sensor *sensor, float *out);
int sensorReadPtsVersion(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePtsDate(TSS_Sensor *sensor, const uint32_t value[3]);
int sensorReadPtsDate(TSS_Sensor *sensor, uint32_t out[3]);
int sensorReadPtsWmmVersion(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWritePtsWmmSet(TSS_Sensor *sensor, const char *value);
int sensorWritePtsForceOutGps(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsForceOutGps(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePtsInitialHeadingTolerance(TSS_Sensor *sensor, float value);
int sensorReadPtsInitialHeadingTolerance(TSS_Sensor *sensor, float *out);
int sensorWritePtsHeadingConsistencyReq(TSS_Sensor *sensor, int32_t value);
int sensorReadPtsHeadingConsistencyReq(TSS_Sensor *sensor, int32_t *out);
int sensorWritePtsHeadingRootErrMul(TSS_Sensor *sensor, float value);
int sensorReadPtsHeadingRootErrMul(TSS_Sensor *sensor, float *out);
int sensorWritePtsHeadingConsistentBias(TSS_Sensor *sensor, float value);
int sensorReadPtsHeadingConsistentBias(TSS_Sensor *sensor, float *out);
int sensorWritePtsStrictBiasEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadPtsStrictBiasEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePinMode0(TSS_Sensor *sensor, uint8_t value);
int sensorReadPinMode0(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePinMode1(TSS_Sensor *sensor, uint8_t value);
int sensorReadPinMode1(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteUartBaudrate(TSS_Sensor *sensor, uint32_t value);
int sensorReadUartBaudrate(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteI2cAddr(TSS_Sensor *sensor, uint8_t value);
int sensorReadI2cAddr(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePowerHoldTime(TSS_Sensor *sensor, float value);
int sensorReadPowerHoldTime(TSS_Sensor *sensor, float *out);
int sensorWritePowerHoldState(TSS_Sensor *sensor, uint8_t value);
int sensorReadPowerHoldState(TSS_Sensor *sensor, uint8_t *out);
int sensorWritePowerInitialHoldState(TSS_Sensor *sensor, uint8_t value);
int sensorReadPowerInitialHoldState(TSS_Sensor *sensor, uint8_t *out);
int sensorFsCfgLoad(TSS_Sensor *sensor);
int sensorWriteFsMscEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadFsMscEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteFsMscAuto(TSS_Sensor *sensor, uint8_t value);
int sensorReadFsMscAuto(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogInterval(TSS_Sensor *sensor, uint64_t value);
int sensorReadLogInterval(TSS_Sensor *sensor, uint64_t *out);
int sensorWriteLogHz(TSS_Sensor *sensor, float value);
int sensorReadLogHz(TSS_Sensor *sensor, float *out);
int sensorWriteLogStartEvent(TSS_Sensor *sensor, const char *value);
int sensorReadLogStartEvent(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteLogStartMotionThreshold(TSS_Sensor *sensor, float value);
int sensorReadLogStartMotionThreshold(TSS_Sensor *sensor, float *out);
int sensorWriteLogStopEvent(TSS_Sensor *sensor, const char *value);
int sensorReadLogStopEvent(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteLogStopMotionThreshold(TSS_Sensor *sensor, float value);
int sensorReadLogStopMotionThreshold(TSS_Sensor *sensor, float *out);
int sensorWriteLogStopMotionDelay(TSS_Sensor *sensor, float value);
int sensorReadLogStopMotionDelay(TSS_Sensor *sensor, float *out);
int sensorWriteLogStopCount(TSS_Sensor *sensor, uint64_t value);
int sensorReadLogStopCount(TSS_Sensor *sensor, uint64_t *out);
int sensorWriteLogStopDuration(TSS_Sensor *sensor, float value);
int sensorReadLogStopDuration(TSS_Sensor *sensor, float *out);
int sensorWriteLogStopPeriodCount(TSS_Sensor *sensor, uint32_t value);
int sensorReadLogStopPeriodCount(TSS_Sensor *sensor, uint32_t *out);
int sensorWriteLogStyle(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogStyle(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogPeriodicCaptureTime(TSS_Sensor *sensor, float value);
int sensorReadLogPeriodicCaptureTime(TSS_Sensor *sensor, float *out);
int sensorWriteLogPeriodicRestTime(TSS_Sensor *sensor, float value);
int sensorReadLogPeriodicRestTime(TSS_Sensor *sensor, float *out);
int sensorWriteLogBaseFilename(TSS_Sensor *sensor, const char *value);
int sensorReadLogBaseFilename(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteLogFileMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogFileMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogDataMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogDataMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogOutputSettings(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogOutputSettings(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogHeaderEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogHeaderEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogFolderMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogFolderMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogImmediateOutput(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogImmediateOutput(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogImmediateOutputHeaderEnabled(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogImmediateOutputHeaderEnabled(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteLogImmediateOutputHeaderMode(TSS_Sensor *sensor, uint8_t value);
int sensorReadLogImmediateOutputHeaderMode(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteBleName(TSS_Sensor *sensor, const char *value);
int sensorReadBleName(TSS_Sensor *sensor, char *out, uint32_t size);
int sensorWriteGpsStandby(TSS_Sensor *sensor, uint8_t value);
int sensorReadGpsStandby(TSS_Sensor *sensor, uint8_t *out);
int sensorWriteGpsLed(TSS_Sensor *sensor, uint8_t value);
int sensorReadGpsLed(TSS_Sensor *sensor, uint8_t *out);

#endif /* __TSS_SENSOR_H__ */