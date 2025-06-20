#include "tss/com/serial.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

#include <stdbool.h>

static int open(void *user_data);
static int close(void *user_data);

static int read(size_t num_bytes, uint8_t *out, void *user_data);
static int peek(size_t start, size_t num_bytes, uint8_t *out, void *user_data);

static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data);
static int peek_until(size_t start, uint8_t value, uint8_t *out, size_t size, void *user_data);

static size_t length(void *user_data);  
static size_t peek_capacity(void *user_data);
static void set_timeout(uint32_t timeout_ms, void *user_data);
uint32_t get_timeout(void *user_data);
static void clear_immediate(void *user_data);
static void clear_timeout(void *user_data, uint32_t timeout_ms);

static int write(const uint8_t *bytes, size_t len, void *user_data);

#if TSS_BUFFERED_WRITES
static int begin_write(void *user_data);
static int end_write(void *user_data);
#endif

void create_serial_com_class(uint8_t port, struct SerialComClass *out)
{
    *out = (struct SerialComClass) {
        .port = {
            .port = port,
        },
        .com = (struct TSS_Com_Class) {
            .user_data = out,
            
            .open = open,
            .close = close,

            .reenumerates = true,
            .auto_detect = serial_com_auto_detect,
            
            .in = {
                .read = read,
                .read_until = read_until,

#if TSS_MINIMAL_SENSOR == 0
                .peek = peek,
                .peek_until = peek_until,
                .length = length,
                .peek_capacity = peek_capacity,
#endif

                .set_timeout = set_timeout,
                .get_timeout = get_timeout,

                .clear_immediate = clear_immediate,
                .clear_timeout = clear_timeout,
            },
            .out = {
                .write = write
            },
        },
    };

#if TSS_BUFFERED_WRITES
    out->com.out.begin_write = begin_write;
    out->com.out.end_write = end_write;
#endif
#if TSS_MINIMAL_SENSOR == 0
    out->in_ring = (struct TSS_Ring_Buf2) {
        .data = out->_in_buffer,
        .capacity = sizeof(out->_in_buffer)
    };
#endif
}

static int write(const uint8_t *bytes, size_t len, void *user_data)
{
    struct SerialComClass *com = user_data;

#if TSS_BUFFERED_WRITES
    uint32_t i = 0;
    while(i < len) {
        while(com->index < sizeof(com->buffer) && i < len) {
            com->buffer[com->index++] = bytes[i++];
        }
        
        //Would buffer overflow, so need to send now.
        if(i < len) {
            end_write(com);
            begin_write(com); //Start again to finish writing
        }
    }

    return com->index >= sizeof(com->buffer);
#else
    serWrite(&com->port, (char*)bytes, len);
    return 0;
#endif
}

#if TSS_BUFFERED_WRITES
static int begin_write(void *user_data)
{
    struct SerialComClass *com = user_data;
    com->index = 0;
    return 0;
}

static int end_write(void *user_data)
{
    struct SerialComClass *com = user_data;
    serWrite(&com->port, (char*)com->buffer, com->index);
    return 0;
}
#endif

static int open(void *user_data)
{
    struct SerialComClass *com = user_data;
    int result = serOpen(com->port.port, 115200, &com->port);
    if(result) return result;
    serConfigBufferSize(&com->port, 4096, 64);
    return 0;
}

static int close(void *user_data)
{
    struct SerialComClass *com = user_data;
    serClose(&com->port);

    return 0;
}

#if TSS_MINIMAL_SENSOR
static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    struct SerialComClass *com = user_data;
    return serRead(&com->port, (char*)out, num_bytes);
}

static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    uint32_t num_read;
    struct SerialComClass *com = user_data;
    tss_time_t start_time = tssTimeGet();

    num_read = 0;

    while(num_read < size && tssTimeDiff(start_time) < com->port.timeout) {
        if(serReadImmediate(&com->port, out, 1)) { //Read one at a time to avoid overreading for now
            num_read++;
            if(*out++ == value) {
                break;
            }
        }
    }

    return num_read;
}
#else

static int _read(size_t num_bytes, uint8_t *out, void *user_data, bool immediate)
{
    size_t i;
    struct SerialComClass *com = user_data;

    //First read data from the peek buffer
    size_t peek_len = ring_size(&com->in_ring);
    for(i = 0; i < peek_len && i < num_bytes; i++) {
        out[i] = ring_pop(&com->in_ring);
    }

    //Then read data from the actual port up to size
    if(immediate) {
        return i + serReadImmediate(&com->port, (char*)(out + i), num_bytes - i);
    }
    else {
        return i + serRead(&com->port, (char*)(out + i), num_bytes - i);
    }
}

static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    return _read(num_bytes, out, user_data, false);
}

static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    uint32_t num_read;
    struct SerialComClass *com = user_data;
    tss_time_t start_time = tssTimeGet();

    num_read = 0;

    while(num_read < size && tssTimeDiff(start_time) < com->port.timeout) {
        if(_read(1, out, com, true)) { //Read one at a time to avoid overreading for now
            num_read++;
            if(*out++ == value) {
                break;
            }
        }
    }

    return num_read;
}

static int peek(size_t start, size_t num_bytes, uint8_t *out, void *user_data)
{
    uint16_t i;
    struct SerialComClass *com = user_data;
    size_t required_length = start + num_bytes;
    size_t len;

    if(required_length > com->in_ring.capacity) {
        return TSS_ERR_INSUFFICIENT_BUFFER;
    }

    //Wait for all requested data to be available
    tss_time_t start_time = tssTimeGet();
    while(length(com) < required_length && tssTimeDiff(start_time) < com->port.timeout);
    len = length(com);

    //Read out as much as can be read.
    for(i = 0; i < num_bytes && i + start < len; i++) {
        out[i] = ring_read(&com->in_ring, i + start);
    }

    return i;
}

static int peek_until(size_t start, uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    uint32_t num_read;
    size_t len;
    bool done;

    struct SerialComClass *com = user_data;
    tss_time_t start_time = tssTimeGet();

    done = false;
    num_read = 0;
    while(!done && num_read < size && num_read + start < com->in_ring.capacity && tssTimeDiff(start_time) < com->port.timeout) {
        len = length(com);
        while(num_read + start < len) { 
            *out = ring_read(&com->in_ring, num_read + start);
            num_read++;
            if(*out == value) {
                done = true;
                break;
            }
            out++;
        }
    }
    
    //Stopped reading because can't peek further, not because no room to fill
    if(!done && num_read < size && num_read + start == com->in_ring.capacity) {
        return TSS_ERR_INSUFFICIENT_BUFFER;
    }

    return num_read;
}

inline static void fill_in_buffer(struct SerialComClass *com) {
    size_t space, start_index, end_index, start_len, read_len;

    space = ring_space(&com->in_ring);
    if(space == 0) return;
    
    //Read filling write index up to either capacity or the read index.
    start_index = ring_index(&com->in_ring, com->in_ring.w_index);
    start_len = com->in_ring.capacity - start_index;
    if(space < start_len) start_len = space; //Can't reach the end, the read index is in front of it

    read_len = serReadImmediate(&com->port, com->_in_buffer + start_index, start_len);

    //Update state variables
    com->in_ring.w_index += read_len;
    space -= read_len;

    //Finished reading all available data
    if(read_len != start_len || space == 0) return;

    //Read filling start of buffer up to the read index.
    //If read_index is after write_index, this naturally won't happen because space will be 0
    //since filled between write and read first.

    //More to read possibly, fill from the start of the buffer now
    end_index = ring_index(&com->in_ring, com->in_ring.r_index);
    read_len = serReadImmediate(&com->port, com->_in_buffer, end_index);

    com->in_ring.w_index += read_len;
}

static size_t length(void *user_data)
{
    struct SerialComClass *com = user_data;
    fill_in_buffer(com);
    return ring_size(&com->in_ring);
}

static size_t peek_capacity(void *user_data)
{
    struct SerialComClass *com = user_data;
    return com->in_ring.capacity;
}

#endif

static void set_timeout(uint32_t timeout_ms, void *user_data)
{
    struct SerialComClass *com = user_data;
    serSetTimeout(&com->port, timeout_ms);
}

uint32_t get_timeout(void *user_data)
{
    struct SerialComClass *com = user_data;
    return com->port.timeout;
}

static void clear_immediate(void *user_data)
{
    uint8_t buffer[40];
    uint8_t len;

    struct SerialComClass *com = user_data;
    ring_clear(&com->in_ring);
    do {
        len = serReadImmediate(&com->port, buffer, sizeof(buffer));
    } while(len > 0);
}

static void clear_timeout(void *user_data, uint32_t timeout_ms)
{
    uint8_t buffer[40];
    uint8_t len;
    tss_time_t start, interval_start;

    struct SerialComClass *com = user_data;
    ring_clear(&com->in_ring);
    
    start = tssTimeGet();
    interval_start = start;
    do {
        len = serReadImmediate(&com->port, buffer, sizeof(buffer));
        if(len > 0) {
            interval_start = tssTimeGet();
        } 
    } while(tssTimeDiff(interval_start) < timeout_ms && tssTimeDiff(start) < com->port.timeout); //How long there has to be no data in the buffer before considering as cleared
}

struct PortEnumerate {
    struct TSS_Com_Class *out;
    TssComAutoDetectCallback cb;
    void *user_data;
    int result;
};

static uint8_t auto_detect(const char *name, uint8_t port, void *user_data)
{
    struct PortEnumerate *params = user_data;

    //TSS_Com_Class is the first element of  SerialComClass, so this is a valid cast
    struct SerialComClass *com = (struct SerialComClass*) params->out;
    create_serial_com_class(port, com);

    if(params->cb != NULL) {
        params->result = params->cb(&com->com, params->user_data);
    }
    else {
        params->result = TSS_AUTO_DETECT_SUCCESS;
    }
    
    if(params->result != TSS_AUTO_DETECT_CONTINUE) {
        return SER_ENUM_STOP;
    }

    return SER_ENUM_CONTINUE;
}

int serial_com_auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *user_data)
{
    //The serEnumerate is also implemented as a callback function, so have to wrap it
    struct PortEnumerate params = {
        .out = out,
        .cb = cb,
        .user_data = user_data,
        .result = TSS_AUTO_DETECT_CONTINUE
    };

    serEnumeratePorts(auto_detect, &params);
    if(params.result == TSS_AUTO_DETECT_CONTINUE) {
        return TSS_AUTO_DETECT_DONE;
    }
    return params.result;
}