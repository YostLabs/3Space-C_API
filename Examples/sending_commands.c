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

    tssCreateSensor(&sensor, com);
    tssInitSensor(&sensor);

    printf("Getting data\n");
    //Retrieve quaternion and accelerometer data
    float quaternion[4];
    float accel[3];
    sensorGetTaredOrientation(&sensor, quaternion);
    sensorGetCorrectedAccelerometerVector(&sensor, accel);

    printf("Quat: %f %f %f %f\n", quaternion[0], quaternion[1], quaternion[2], quaternion[3]);
    printf("Accel: %f %f %f\n", accel[0], accel[1], accel[2]);

    sensorSoftwareReset(&sensor);

    uint64_t time;
    sensorGetTimestamp(&sensor, &time);
    printf("Time: %llu\n", time);

    sensorCleanup(&sensor);

    return 0;
}