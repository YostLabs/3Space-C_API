#ifndef __TSS_VERSION_H__
#define __TSS_VERSION_H__

#include <stdint.h>
#include <stdbool.h>
#include "tss_sensor.h"

#define TSS_FIRMWARE_UPLOAD_IN_PROGRESS 0
#define TSS_FIRMWARE_UPLOAD_COMPLETE 1

struct TSS_Firmware_Uploader {
    TSS_Sensor *sensor;

    uint8_t *buffer;

    //Minimum size of 16, must be a power of 2
    size_t buffer_size;
    size_t index;

    int status;
    uint32_t packet_count;

    //Configurable timeouts
    uint32_t timeout_erase_ms; //Defaults to 20000
    uint32_t timeout_program_ms; //Defaults to 5000
    uint32_t timeout_load_firmware_ms; //Defaults to 2000
};

int tssFirmwareUploaderCreate(TSS_Sensor *sensor, uint8_t *buffer, size_t buffer_size, struct TSS_Firmware_Uploader *out);
int tssFirmwareUpload(struct TSS_Firmware_Uploader *uploader, char *data, size_t len);

#endif /* __TSS_VERSION_H__ */