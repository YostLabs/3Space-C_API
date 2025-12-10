#include "tss/com/serial.h"
#include "tss/api/sensor.h"
#include "tss/sys/time.h"

#include <stdio.h>

static enum TSS_DataCallbackState onStreamingPacket(TSS_Sensor *sensor)
{
    static int count = 0;
    float quat[4], primary_accel[3];
    sensorProcessDataStreamingCallbackOutput(sensor, quat, primary_accel);
    printf("Streaming Result %d [%lu]:\n Quat: %f %f %f %f\nAccel: %f %f %f\n\n",
        count++, sensorGetLastHeader(sensor).timestamp,
        quat[0], quat[1], quat[2], quat[3],
        primary_accel[0], primary_accel[1], primary_accel[2]);
    return TSS_DataCallbackStateProcessed;
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

    if(com->open(com->user_data)) {
        printf("Failed to open port.\r\n");
        return -1;
    }

    tssCreateSensor(&sensor, com);
    err = tssInitSensor(&sensor);
    if(err) {
        printf("Failed to initialize sensor: %d\n", err);
        return -1;
    }

    //Set stream slots to Orientation and Accel data
    sensorWriteStreamSlots(&sensor, "0,39");

    //Read out the stream slots to check if set
    char stream_slots[150];
    sensorReadStreamSlots(&sensor, stream_slots, sizeof(stream_slots));
    printf("Stream Slots: %s\n", stream_slots);

    //Getting a singular packet based on the stream slots
    float quat[4], accel[3];
    sensorStreamingGetPacket(&sensor, quat, accel);
    printf("Packet Response:\n Quat: %f %f %f %f\nAccel: %f %f %f\n\n",
        quat[0], quat[1], quat[2], quat[3],
        accel[0], accel[1], accel[2]);
    
    //Asynchronous streaming
    sensorWriteStreamHz(&sensor, 2000);
    //sensorWriteStreamInterval(&sensor, 20000); //This is equivalent to above

    //Setup timing information as well.
    sensorWriteHeaderTimestamp(&sensor, 1);
    sensorWriteTimestamp(&sensor, 0);
    sensorStreamingStart(&sensor, onStreamingPacket);
    tss_time_t start_time = tssTimeGet();
    while(tssTimeDiff(start_time) < 5000) {
        //This function parses only 1 response at a time. It returns 
        //true if a packet was parsed. It is a good idea to
        //parse all available packets before continuing on to
        //your main loop to ensure all packets are processed.
        //However, if your device runs too slow that the streaming
        //callback can't keep up with the rate packets are coming in,
        //you may want to limit the number of updates this loop can do
        //to avoid infinite loops (and also probably decrease your streaming speed)
        while(sensorUpdateStreaming(&sensor));

        //Note: As long as TSS_MINIMAL_SENSOR is 0 in the config (default),
        //you can continue to send other sensor commands and/or settings
        //while streaming is active.
        
    }
    sensorStreamingStop(&sensor);

    sensorCleanup(&sensor);

    return 0;
}