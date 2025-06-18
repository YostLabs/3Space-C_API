#include "tss/api/header.h"
#include "tss/sys/config.h"

struct TSS_Header_Info tssHeaderInfoFromBitfield(uint8_t bitfield)
{
    return (struct TSS_Header_Info) {
        .bitfield = bitfield,
        .size = tssHeaderSizeFromBitfield(bitfield)
    };
}

uint8_t tssHeaderPosFromBitfield(uint8_t bitfield, uint8_t bit) 
{
    uint8_t pos = 0;
    while((bit & 1) == 0) {
        bit >>= 1;
        pos += tssHeaderSizeFromBitfield(bitfield & bit);
    }
    return pos;
}

uint8_t tssHeaderSizeFromBitfield(uint8_t bitfield)
{
    uint8_t size = 0;
    if(bitfield & TSS_HEADER_STATUS_BIT) {
        size += 1;
    }
    if(bitfield & TSS_HEADER_TIMESTAMP_BIT) {
        size += 4;
    }
    if(bitfield & TSS_HEADER_ECHO_BIT) {
        size += 1;
    }
    if(bitfield & TSS_HEADER_CHECKSUM_BIT) {
        size += 1;
    }
    if(bitfield & TSS_HEADER_SERIAL_BIT) {
        size += 4;
    }
    if(bitfield & TSS_HEADER_LENGTH_BIT) {
        size += 2;
    }
    return size;
}

void tssHeaderFromBytes(const struct TSS_Header_Info *info, uint8_t *data, struct TSS_Header *out)
{
    if(info->bitfield & TSS_HEADER_STATUS_BIT) {
        out->status = *data++;
    }
    if(info->bitfield & TSS_HEADER_TIMESTAMP_BIT) {
        out->timestamp = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
        data += 4;
    }
    if(info->bitfield & TSS_HEADER_ECHO_BIT) {
        out->echo = *data++;
    }
    if(info->bitfield & TSS_HEADER_CHECKSUM_BIT) {
        out->checksum = *data++;
    }
    if(info->bitfield & TSS_HEADER_SERIAL_BIT) {
        out->serial = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
        data += 4;
    }
    if(info->bitfield & TSS_HEADER_LENGTH_BIT) {
        out->length = data[0] | data[1] << 8;
        data += 2;
    }
}