#include "tss/com/spi.h"
#include "tss/api/sensor.h"

#include <stdio.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define GPIO_CHIP "/dev/gpiochip0"
#define CS_LINE 25 // GPIO pin number for Chip Select (CS)

//If setting up the IRQ pins for use, make sure that the setting
//pin_mode0 is set to 8 (TransactionIRQ mode) or the data_available and data_loaded pins will not function correctly.
#define DATA_AVAILABLE_LINE -1 // GPIO pin number for Data Available (set to -1 if not used)
#define DATA_LOADED_LINE -1 // GPIO pin number for Data Loaded (set to -1 if not used)

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

    //Check the manual for correct SPI speeds. The possible SPI speeds are
    //based on the cpu_speed setting of the sensor. This default value is the
    //maximum speed for the default cpu_speed of 96MHz. If you are encountering
    //any communication errors, try lowering this speed, or increasing the sensors
    //cpu_speed setting.
    //https://yostlabs.com/v3usermanual
    create_spi_com_class(id, 5000000, &ser);
    com = (struct TSS_Com_Class*) &ser;
    
    if(tss_com_open(com)) {
        printf("Failed to open port.\r\n");
        return -1;
    }

    //Must be called after opening the com class
    spiConfigurePinMode(&ser.device, DATA_AVAILABLE_LINE, DATA_LOADED_LINE);

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

    //It is recommended to do software resets when using the transactional protocol
    //without the IRQ pins because the reset may cause the IRQ lines to fluctuate,
    //causing expected data and potential delays. It still works, just might take longer
    //to reconnect.
    int loaded_pin = ser.device.id.data_available_line_num;
    int available_pin = ser.device.id.data_loaded_line_num;
    spiConfigurePinMode(&ser.device, -1, -1); //Disable IRQs for reset
    int reset_err = sensorSoftwareReset(&sensor);
    spiConfigurePinMode(&ser.device, loaded_pin, available_pin); //Re-enable IRQs
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