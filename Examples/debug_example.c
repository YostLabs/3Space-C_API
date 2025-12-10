/*
* Debug Example:
* Sensor debug messages has 2 modes controlled by the debug_mode setting.
*
* debug_mode=0 is default, and is called buffered mode. In this mode, generated
* debug messages will be stored for later use and can be accessed via the commands
* sensorGetDebugMessageCount and sensorGetDebugMessage. While debug messages are available,
* the sensor's LED will blink Magenta if debug_led=1 (default).
*
* debug_mode=1 will cause any buffered debug messages to immediately be output, and all
* newly generated messages while debug_mode=1 to be output on generation as well. This debug_mode
* is only supported when TSS_MINIMAL_SENSOR in the config is 0 (default). To receive these messages,
* a callback function must be registered that parses the message on generation.
*
* For more information on debug messages, such as debug_level and debug_module, see the user manual.
*
* NOTE: We generally suggest just sticking to using debug_mode=0. debug_mode=1 requires additional
* logic for parsing and if commited may cause the sensor to lock up on startup if there are too many
* messages immediately output on startup.
*/

#include "tss/com/serial.h"
#include "tss/api/sensor.h"
#include "tss/sys/time.h"
#include "tss/constants.h"

#include <stdio.h>

//Simplest debug message callback. But uses a large character buffer 
//to read the whole message at once.
static enum TSS_DataCallbackState onDebugMessage(TSS_Sensor *sensor)
{
    char buffer[TSS_DEBUG_MESSAGE_MAX_SIZE];
    int num_read = sensorProcessDebugCallbackOutput(sensor, buffer, sizeof(buffer));
    if(num_read < 0) return TSS_DataCallbackStateError;
    printf("%.*s", num_read, buffer);
    return TSS_DataCallbackStateProcessed;
}

//Reading out the debug message in smaller chunks. The advantage
//of doing this is just to use a smaller buffer.
// static enum TSS_DataCallbackState onDebugMessage(TSS_Sensor *sensor)
// {
//     char buffer[10];
//     int num_read;
//     do {
//         num_read = sensorProcessDebugCallbackOutput(sensor, buffer, sizeof(buffer));
//         if(num_read < 0) {
//             return TSS_DataCallbackStateError;
//         }
//         printf("%.*s", num_read, buffer);
//     } while(num_read != 0);

//     return TSS_DataCallbackStateProcessed;
// }

//If you don't want to do anything with the debug message, just ignore it
// static enum TSS_DataCallbackState onDebugMessage(TSS_Sensor *sensor)
// {
//     return TSS_DataCallbackStateIgnored;
// }


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
    sensorSetDebugCallback(&sensor, onDebugMessage);
    err = tssInitSensor(&sensor);
    if(err) {
        printf("Failed to initialize sensor: %d\n", err);
        return -1;
    }

    //When debug_mode=0
    uint16_t num_messages = 0;
    char message[TSS_DEBUG_MESSAGE_MAX_SIZE];
    sensorGetDebugMessageCount(&sensor, &num_messages);
    printf("Outputting %d buffered debug messages.\n", num_messages);
    for(uint32_t i = 0; i < num_messages; i++) {
        sensorGetDebugMessage(&sensor, message, sizeof(message));
        printf("%s\n", message);
    }

    printf("Enabling Immediate Debug Mode...\n");
    sensorWriteDebugMode(&sensor, 1);
    //Debug messages will now be output as they are generated via the callback function
    //set with sensorSetDebugCallback. Note that they will only be output when another
    //sensor function is called that causes it to check its communication interface.
    //NOTE: The generated debug messages will only be detected when the sensor object goes to attempt to read
    //something else. So sending commands, streaming, any sensor activity really, is required to receive the messages.
    //If you simply set debug_mode=1 and register the callback and enter a loop that does nothing, generated messages
    //will not be output.

    //There aren't many ways to gurantee a debug message is generated, as they are mostly warning/errors.
    //The easiest way are info level messages from the self_test or GPS. Info level is not enabled by default,
    //so will do that here and generate a message to show an example of an immediate debug message.
    uint32_t debug_level, debug_module;
    sensorReadDebugLevel(&sensor, &debug_level);
    sensorReadDebugModule(&sensor, &debug_module);

    sensorWriteDebugLevel(&sensor, 0x07);
    sensorWriteDebugModule(&sensor, 0xFFFFFFFF);

    uint32_t result;
    sensorSelfTest(&sensor, &result);

    //Restore previous settings
    sensorWriteDebugLevel(&sensor, debug_level);
    sensorWriteDebugModule(&sensor, debug_module);
    sensorWriteDebugMode(&sensor, 0);

    sensorCleanup(&sensor);

    return 0;
}