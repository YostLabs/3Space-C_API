#include "tss/com/spi.h"
#include "tss/api/sensor.h"

#include <stdio.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define GPIO_CHIP "/dev/gpiochip0"
#define CS_LINE 25 // GPIO pin number for Chip Select (CS)

int main() {
    int err;
    struct SpiComClass ser;
    struct TSS_Com_Class *com;
    
    struct TSS_Sensor sensor;

    SpiPortId id = {
        .device_name = SPI_DEVICE,
        .chip_path = GPIO_CHIP,
        .cs_line_num = CS_LINE, // GPIO pin number for Chip Select (CS)
    };

    create_spi_com_class(id, 10000000, &ser);

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

    printf("Getting data\n");
    //Retrieve quaternion and accelerometer data
    float quaternion[4];
    float accel[3];
    sensorGetTaredOrientation(&sensor, quaternion);
    sensorGetCorrectedAccelerometerVector(&sensor, accel);

    printf("Quat: %f %f %f %f\n", quaternion[0], quaternion[1], quaternion[2], quaternion[3]);
    printf("Accel: %f %f %f\n", accel[0], accel[1], accel[2]);

    int reset_err = sensorSoftwareReset(&sensor);
    if(reset_err) {
        printf("Failed to reset sensor: %d\n", reset_err);
        return -1;
    }

    uint64_t time;
    sensorGetTimestamp(&sensor, &time);
    printf("Time: %llu\n", time);

    sensorCleanup(&sensor);

    return 0;
}