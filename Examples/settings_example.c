/*
* Shows the multiple ways of reading and writing settings.
*/

#include "tss/com/serial.h"
#include "tss/api/sensor.h"

#include <stdio.h>
#include "tss/sys/stdinc.h"

//Callback used for Aggregate/Query settings
static enum TSS_SettingsCallbackState getSettingsCallback(struct TSS_GetSettingsCallbackInfo info, void *user_data)
{
    //Whatever type you pass into the user_data when registering the callback
    const char *label = user_data;
    
    //Parse keys that output strings
    if(strcmp(info.key, "version_firmware") == 0 || strcmp(info.key, "version_hardware") == 0) {
        //The info object contains the information need to read the setting
        char out_str[50];

        //When using tssReadParams to read out the response, make sure to pass the checksum from the info
        //field to allow the sensor object to validate the checksum after this callback function returns.
        int err = tssReadParams(info.com, info.setting->out_format, info.checksum, out_str, sizeof(out_str));
        if(err) {
            printf("Failed to read setting: %s %d\n", info.key, err);
            return TSS_SettingsCallbackStateError;
        }
        printf("%s Key: %s=%s\n", label, info.key, out_str);

        //The sensor object does not have to handle reading the value since it was read here.
        return TSS_SettingsCallbackStateProcessed;
    }
    //Keys we won't parse the response of
    else {
        printf("%s Key: %s\n", label, info.key);

        //The sensor object does need to handle reading past the value since it was not read here.
        return TSS_SettingsCallbackStateIgnored;
    }
}



int main() {
    int err;
    struct SerialComClass ser;
    struct TSS_Com_Class *com;
    
    struct TSS_Sensor sensor;

    err = serial_com_auto_detect((struct TSS_Com_Class*)&ser, NULL, NULL);
    if(err != TSS_AUTO_DETECT_SUCCESS) {
        printf("Failed to detect com port\n");
        return -1;
    }

    com = (struct TSS_Com_Class*) &ser;

    if(tss_com_open(com)) {
        printf("Failed to open port.\r\n");
        return -1;
    }

    tssCreateSensor(&sensor, com);
    err = tssInitSensor(&sensor);
    if(err) {
        printf("Failed to initialize sensor: %d\n", err);
        return -1;
    }

    //Most settings can be read/written individually using simple functions
    //the same as one would use commands.
    char original_euler_order[10], new_euler_order[10];
    sensorReadEulerOrder(&sensor, original_euler_order, sizeof(original_euler_order));
    printf("Euler Order: %s\n", original_euler_order);
    sensorWriteEulerOrder(&sensor, "XYZ");
    sensorReadEulerOrder(&sensor, new_euler_order, sizeof(new_euler_order));
    printf("New Order: %s\n", new_euler_order);
    sensorWriteEulerOrder(&sensor, original_euler_order); //Revert back

    //---------------------------------------------------------------------------------

    //Settings can also be read/written using the read/write setting functions directly to 
    //take advantage of the key based settings protocol
    uint8_t timestamp_enabled, serial_enabled, status_enabled, echo_enabled;
    sensorWriteHeader(&sensor, 0); //Some of these will be forced back on by the Managed API, such as echo
    err = sensorReadSettings(&sensor, "header_timestamp;header_serial;header_status;header_echo", &timestamp_enabled, &serial_enabled, &status_enabled, &echo_enabled);
    if(err) {
        printf("Error reading settings: %d\n", err);
        return -1;
    }
    printf("Timestamp Enabled: %d  Serial Enabled: %d  Status Enabled: %d  Echo Enabled: %d\n", timestamp_enabled, serial_enabled, status_enabled, echo_enabled);
    
    //Writing multiple settings generically. Takes an Array of Keys and an Array of pointers to values to set those keys to.
    uint8_t enabled = 1;
    sensorWriteSettings(&sensor, (const char*[]) { "header_timestamp", "header_status" }, 2, (const void*[]) { &enabled, &enabled });
    sensorReadSettings(&sensor, "header_timestamp;header_serial;header_status;header_echo", &timestamp_enabled, &serial_enabled, &status_enabled, &echo_enabled);
    printf("Timestamp Enabled: %d  Serial Enabled: %d  Status Enabled: %d  Echo Enabled: %d\n", timestamp_enabled, serial_enabled, status_enabled, echo_enabled);

    //---------------------------------------------------------------------------------
    
    //Some settings are aggregate settings, or query settings. These settings are readonly and
    // can return a varying number of keys when read. For example ?all reads all settings, or
    // ?{accel} reads all settings with 'accel' in their name. To read these out, since the number
    // of keys in the response is unknown, a callback function is used instead for each key.
    
    //Provide the callback for each key, with optional user data that will be passed to the callback
    sensorReadAllSettings(&sensor, getSettingsCallback, "All");

    //This function actually works with any setting read string, but queries {} have to be used
    //in this function to potentially parse. Generally though, this is just a callback method of doing
    //a read.
    sensorReadSettingsQuery(&sensor, "{odr}", getSettingsCallback, "Odr");
    sensorReadSettingsQuery(&sensor, "header_timestamp;header_serial;header_status;header_echo", getSettingsCallback, "Normal");
    sensorReadSettingsQuery(&sensor, "{version}", getSettingsCallback, "Normal2");

    //NOTE: For setting changes to persist between power cycles, the following must be called.
    //sensorCommitSettings(&sensor);

    sensorCleanup(&sensor);

    return 0;
}