/*
* Firmware upload example
*/

#include <stdio.h>

#include "tss/com/serial.h"
#include "tss/api/sensor.h"
#include "tss/tools/firmware.h"

#include <windows.h>

int main()
{
    int err;

    //-----------------------Discover Com Port and configure-----------------------------
    struct SerialComClass ser;
    if(serial_com_auto_detect(&ser.com, NULL, NULL) != TSS_AUTO_DETECT_SUCCESS) {
        printf("Failed to detect sensor.\n");
    }

    if(ser.com.open(&ser)) {
        printf("Failed to open port.\r\n");
        return 1;
    }

    printf("Successfully opened port COM%d\r\n", ser.port.port);
    ser.com.in.set_timeout(1000, ser.com.user_data);
    ser.com.in.clear_timeout(ser.com.user_data, 5);

    //------------------------------Create Sensor Object-------------------------------
    TSS_Sensor sensor_base;
    TSS_Sensor *sensor = &sensor_base;
    printf("Initializing sensor\n");
    createTssSensor(sensor, &ser.com);
    initTssSensor(sensor);

    //------------------------------Enter Bootloader--------------------------------------
    uint8_t active;
    sensorBootloaderIsActive(sensor, &active);
    if(!active) {
        printf("Entering Bootloader...\n");
        sensorEnterBootloader(sensor);
    }

    //--------------------------Create Firmware Uploader--------------------------------
    //The smallest this value can be is 32, the largest effective size is 8192. The larger the buffer, the faster the upload.
    //This must be a power of 2
    uint8_t buffer[8192];
    struct TSS_Firmware_Uploader uploader;
    err = tssFirmwareUploaderCreate(sensor, buffer, sizeof(buffer), &uploader);
    if(err) {
        printf("Failed to create firmware uploader: %d\n", err);
    }

    //--------------------------Open New Firmware Source---------------------------------
    FILE *fptr;
    printf("Opening firmware file.\n");
    fptr = fopen("Examples/Application.xml", "r");
    if(fptr == NULL) {
        printf("Failed to load firmware file...\n");
        return -1;
    }

    //---------------Feed the new Firmware to the uploader until complete------------------
    printf("Starting firmware upload process.\n");
    char c;
    size_t last_packet_num = 0;
    err = 0;
    while(!feof(fptr) && err != 1) {
        c = fgetc(fptr);
        err = tssFirmwareUpload(&uploader, &c, 1);
        if(err < 0) {
            printf("Failed to upload firmware: %d\n", err);
            fclose(fptr);
            return -1;
        }
        if(uploader.packet_count != last_packet_num) {
            last_packet_num = uploader.packet_count;
            printf("Uploaded packet %d\n", last_packet_num);
        }
    }

    //----------------------Print out new firmware version----------------------------
    //Note: This only works if the com class used is compatible with auto reconnect

    char version[100];
    printf("Checking firmware version...\n");
    sensorReadVersionFirmware(sensor, version, sizeof(version));
    printf("Version: %s\n", version);
    

    //----------------------------Cleanup---------------------------
    fclose(fptr);
    sensorCleanup(sensor);
    printf("Done!\n");
}
