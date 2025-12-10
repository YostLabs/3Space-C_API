/*
* This example shows reading commands via using the Core API and foregoing
* the sensor API entirely. Doing this gives up multiple advantages of the
* sensor API, but does allow more direct control over the process for
* potential speed gains if your application is simple enough and can handle
* less error handling.
*/

#include "tss/com/serial.h"
#include "tss/api/sensor.h"

#include <stdio.h>

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

    if(com->open(com->user_data)) {
        printf("Failed to open port.\r\n");
        return -1;
    }

    const struct TSS_Command *quat_command = tssGetCommand(0);
    const struct TSS_Command *accel_command = tssGetCommand(39);

    float quaternion[4];
    float accel[3];

    //Get the quaternion without the header
    tssWriteCommand(com, false, quat_command, NULL);
    tssReadCommand(com, quat_command, quaternion);

    printf("Quat: %f %f %f %f\n", quaternion[0], quaternion[1], quaternion[2], quaternion[3]);

    //Configure the header
    struct TSS_Header_Info header_format = tssHeaderInfoFromBitfield(TSS_HEADER_TIMESTAMP_BIT | TSS_HEADER_ECHO_BIT);
    
    struct TSS_Setting_Response set_setting_response;
    tssSetSettingsWrite(com, false, (const char*[]) { "header" }, 1, (const void*[]) { &header_format.bitfield });
    tssSetSettingsRead(com, &set_setting_response);

    printf("Set Header Response: Err - %d  Num Success - %d\n", set_setting_response.error, set_setting_response.num_success);
    if(set_setting_response.error) {
        printf("Failed to set header setting.\n");
        return -1;
    }

    uint16_t num_read = 0;
    uint8_t header_bitfield = 0;
    tssGetSettingsWrite(com, false, "header");
    tssGetSettingsRead(com, &num_read, &header_bitfield);
    printf("Num Read: %d Header Bitfield: 0x%02x\n", num_read, header_bitfield);

    //Now get the accel WITH the header since the header is configured
    struct TSS_Header header;
    tssWriteCommand(com, true, accel_command, NULL);
    tssReadHeader(com, &header_format, &header);
    tssReadCommand(com, accel_command, accel);

    printf("Header: Echo - %d  Timestamp - %lu\n", header.echo, header.timestamp);
    printf("Accel: %f %f %f\n", accel[0], accel[1], accel[2]);

    return 0;
}