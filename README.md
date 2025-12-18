<center><h2>C API and Resources for Yost Labs 3.0 Threespace sensors.</h2></center>

The C API for Yost Labs V3 Threespace Sensors is designed to be usable on a wide variety of platforms in both C and C++. It allows access to a sensor object that can be used for directly calling commands, configuring settings, and handling streaming. It will also handle any potential communication errors/data misalignment, as well as debug messages. Lower level functions for communicating directly with the sensor without any overhead caused by the sensor object can also be used.

## Key Design Goals
* No dynamic memory allocation
* Minimal RAM usage
  * All memory requirements are explicit and caller-provided
* Platform-agnostic
  * Suitable for bare-metal environments
  * Compatible with both little-endian and big-endian architectures
* Minimal standard library dependencies
  * Required standard library functionality can be supplied by the user or replaced via provided alternatives

## Installation
The simplest use is to download the project and drop in the `include` folder which contains all the header information and the `api` folder which contains all the source. Then just modify `include\tss\sys\config.h` with any desired settings, and add the source files to your current build system.

The API can also be built as a DLL/Shared Object. When doing so, make sure to make define `TSS_API_EXPORTS` as compile definition to export the functions. The example [Cmake](./CMakeLists.txt) shows this being done.

### Required Modifications
#### Communication Class
To communicate with the sensor the caller must provide a Communication Class, known as a Com Class, for the sensor object to use. A Com Class is a structure containing a set of functions used for communication and any necessary data to execute. This provides a way to interface any device with the API. For example, this could be providing a way for the sensor object to access a microcontrollers UART or I2C subsystems.

This project provides some Communication Classes in the [communication folder](./communication/). Currently, only a Windows Serial com class is available. As these vary by operating system, you may have to implement one for your OS. If there is a specific communication class you need, feel free to let us know at techsupport@yostlabs.com to see if we can assist.

For more information about creating a Communication Class, check out the [wiki](https://github.com/YostLabs/3Space-C_API/wiki/Communication-Class) guide.

### Optional Modifications
There is a config file located at [include\tss\sys\config.h](./include/tss/sys/config.h) with various options.

| Name | Default | Description |
|------|-------------|---------|
| TSS_STDC_AVAILABLE | Enabled | Indicates that the standard C library is available for use |
| TSS_BUFFERED_WRITES | Enabled | All commands will be fully built in a user provided buffer before being sent out over the communication interface. If disabled, commands will be sent as they are generated instead of buffered, possibly resulting in multiple transactions per command. For I2C and SPI this should stay enabled to reduce communication overhead. Disabling this may increase USB/Uart speed, and will reduce required buffer space. |
| TSS_ENDIAN_OVERRIDE | TSS_ENDIAN_RUN_TIME | Controls how data from the sensor is interpreted. Changing this to TSS_ENDIAN_LITTLE or TSS_ENDIAN_BIG will increase speeds by removing the run time checks for endianess. |
| TSS_MINIMAL_SENSOR | False | If enabled, a different set of sensor functions will be compiled in that removes the various checks for data integrity, as well as the requirement of implementing peek functions in the Communication Class. This will increase speed, but remove safety features. It is generally not recommended to enable this. NOTE: This feature currently has some compilation errors that will be fixed in a future update. |

## Basic Usage
The API follows a common pattern of creating a communication object, creating a sensor object from that communication object, and then calling functions by passing that sensor object. Functions return integer errors, and the caller passes in addresses for where to store the actual resulting data. This structure allows the caller to have complete control over memory creation and handle functions that return multiple types. This makes the API safe to use on platforms where memory management is a concern.

Here is a basic example of retrieving data from a sensor:
```C
#include "tss/com/serial.h"
#include "tss/api/sensor.h"

#include <stdio.h>

int main() {
    int err;

    struct SerialComClass ser;
    struct TSS_Com_Class *com;
    struct TSS_Sensor sensor;

    //Create a serial object from COM17
    uint8_t COM_PORT = 17;
    create_serial_com_class(COM_PORT, &ser);
    com = (struct TSS_Com_Class*) &ser;

    err = tss_com_open(com);
    if(err) {
        printf("Failed to open port: %d\n", err);
        return -1;
    }

    tssCreateSensor(&sensor, com);
    err = tssInitSensor(&sensor);
    if(err) {
        printf("Failed to initialize sensor: %d\n", err);
        return -1;
    }
    
    float quaternion[4];
    err = sensorGetTaredOrientation(&sensor, quaternion);
    if(err) {
        printf("Failed to get quaternion: %d\n", err);
        return -1;
    }

    printf("Quat: %f %f %f %f\n", quaternion[0], quaternion[1], quaternion[2], quaternion[3]);

    sensorCleanup(&sensor);
    return 0;
}
```

You can view  the [Examples folder](./Examples/) for more information.\
Other examples include:
* Streaming Data
* Uploading Firmware
* Utilizing the lower level API
* etc...

## Support
If you have any questions reach out to techsupport@yostlabs.com or open an [Issue](https://github.com/YostLabs/3Space-C_API/issues).