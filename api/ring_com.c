#include "tss/com/ring_com.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

static int open(void *user_data);
static int close(void *user_data);

static int read(size_t num_bytes, uint8_t *out, void *user_data);
static int peek(size_t start, size_t num_bytes, uint8_t *out, void *user_data);

static int read_until_wrap(uint8_t value, uint8_t *out, size_t size, void *user_data);
static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data);
static int peek_until(size_t start, uint8_t value, uint8_t *out, size_t size, void *user_data);

static size_t length(void *user_data);  
static size_t peek_capacity(void *user_data);
static void set_timeout(uint32_t timeout_ms, void *user_data);
uint32_t get_timeout(void *user_data);
static void clear_immediate_wrap(void *user_data);
static void clear_immediate(void *user_data);
static void clear_timeout_wrap(void *user_data, uint32_t timeout_ms);
static void clear_timeout(void *user_data, uint32_t timeout_ms);

static int write(const uint8_t *bytes, size_t len, void *user_data);

#if TSS_BUFFERED_WRITES
static int begin_write(void *user_data);
static int end_write(void *user_data);
#endif

static int auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *user_data);

int tssCreateRingCom(const struct TSS_Com_Class *child, uint8_t *read_buf, size_t read_size, 
    uint8_t *write_buf, size_t write_size, struct TSS_Ring_Com_Class *out)
{
    if(!TSS_RING_POW_2(read_size)) {
        return TSS_ERR_INVALID_SIZE;
    }

    //NOTE: Multiple functions have two version, one where the ringCom
    //implements the logic and one where it just calls the childs version

    *out = (struct TSS_Ring_Com_Class) {
        .child = child,
        .read_ring = {
            .capacity = read_size,
            .data = read_buf
        },
        .write_buffer = write_buf,
        .write_buffer_size = write_size,

        .base = {
            .open = open,
            .close = close,
            .reenumerates = child->reenumerates,
            .user_data = out,
            .in = {
                .set_timeout = set_timeout,
                .get_timeout = get_timeout,
                .clear_immediate = (child->in.clear_immediate) ? clear_immediate_wrap : clear_immediate,
                .clear_timeout = (child->in.clear_timeout) ? clear_timeout_wrap :  clear_timeout,
                .read = read,
                .read_until = (child->in.read_until) ? read_until_wrap : read_until,
#if !(TSS_MINIMAL_SENSOR)
                .peek = peek,
                .peek_until = peek_until,
                .peek_capacity = peek_capacity,
                .length = length
#endif
            },
            .out = {
                .write = write,
#if TSS_BUFFERED_WRITES                
                .begin_write = begin_write,
                .end_write = end_write
#endif
            }
        }
    };

    //Only populate if the communication object being wrapped has one
    if(child->auto_detect) {
        out->base.auto_detect = auto_detect;
    }

    return TSS_SUCCESS;
}


//--------------------------CUSTOM WRITE BEHAVIOR----------------------------------
#if TSS_BUFFERED_WRITES
static int begin_write(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    com->write_buffer_index = 0;
    return TSS_SUCCESS;
}

static int end_write(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->out.write(com->write_buffer, com->write_buffer_index, com->child->user_data);
}
#endif

static int write(const uint8_t *bytes, size_t len, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
#if TSS_BUFFERED_WRITES
    uint32_t i = 0;
    while(i < len) {
        while(com->write_buffer_index < com->write_buffer_size && i < len) {
            com->write_buffer[com->write_buffer_index++] = bytes[i++];
        }
        
        //Previous loop ended because buffer would overflow, 
        //so need to send now and start buffering again.
        if(i < len) {
            end_write(com);
            begin_write(com); //Start again to finish writing
        }
    }

    return com->write_buffer_index >= com->write_buffer_size;
#else
    return com->child->out.write(bytes, len, com->child->user_data);
#endif    
}

#if !(TSS_MINIMAL_SENSOR)
//--------------------------CUSTOM READ BEHAVIOR----------------------------------
static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    size_t peek_len, i;
    peek_len = ring_size(&com->read_ring);
    for(i = 0; i < peek_len && i < num_bytes; i++) {
        out[i] = ring_pop(&com->read_ring);
    }

    return i + com->child->in.read(num_bytes - i, out + i, com->child->user_data);   
}

//Utilizes the read until of the child class
static int read_until_wrap(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    size_t peek_len, num_read;

    peek_len = ring_size(&com->read_ring);
    num_read = 0;
    while(num_read < peek_len && num_read < size) {
        *out = ring_pop(&com->read_ring);
        num_read++;
        if(*out == value) {
            return num_read;
        }
        out++;
    }

    num_read += com->child->in.read_until(value, out, size - num_read, com->child->user_data);
    return num_read;   
}

//Works without child class having a read until implemented.
static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    size_t peek_len, num_read;
    tss_time_t start_time;
    uint32_t timeout;

    timeout = get_timeout(com);
    start_time = tssTimeGet();
    peek_len = ring_size(&com->read_ring);
    num_read = 0;
    while(num_read < peek_len && num_read < size) {
        *out = ring_pop(&com->read_ring);
        num_read++;
        if(*out == value) {
            return num_read;
        }
        out++;
    }

    //Want to be able to poll instantly, will change back after
    com->child->in.set_timeout(0, com->child->user_data);
    while(num_read < size && tssTimeDiff(start_time) < timeout) {
        if(com->child->in.read(1, out, com->child->user_data)) {
            num_read++;
            if(*out == value) {
                break;
            }
            out++;
        }
    }
    com->child->in.set_timeout(timeout, com->child->user_data);

    return num_read;
}

static int peek(size_t start, size_t num_bytes, uint8_t *out, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    size_t required_length, len, i;
    tss_time_t start_time;
    uint32_t timeout;

    required_length = start + num_bytes;

    if(required_length > com->read_ring.capacity) {
        return TSS_ERR_INSUFFICIENT_BUFFER;
    }

    timeout = com->child->in.get_timeout(com->child->user_data);
    start_time = tssTimeGet();
    while((len = length(com)) < required_length && tssTimeDiff(start_time) < timeout);

    //Read out as much as can
    for(i = 0; i < num_bytes && i + start < len; i++) {
        out[i] = ring_read(&com->read_ring, i + start);
    }

    return i;
}

static int peek_until(size_t start, uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    size_t num_read, len;
    tss_time_t start_time;
    uint32_t timeout;
    bool done;

    num_read = 0;
    done = false;
    timeout = com->child->in.get_timeout(com->child->user_data);
    start_time = tssTimeGet();
    while(!done && num_read < size && num_read + start < com->read_ring.capacity && tssTimeDiff(start_time) < timeout) {
        len = length(com);
        while(num_read + start < len) {
            *out = ring_read(&com->read_ring, num_read + start);
            num_read++;
            if(*out == value) {
                done = true;
                break;
            }
            out++;
        }
    }

    //Stopped reading because can't peek further, not because no room to fill.
    if(!done && num_read < size && num_read + start == com->read_ring.capacity) {
        return TSS_ERR_INSUFFICIENT_BUFFER;
    }

    return num_read;
}

inline static void fill_in_buffer(struct TSS_Ring_Com_Class *com)
{
    size_t space, start_index, end_index, start_len, read_len;
    uint32_t timeout;

    space = ring_space(&com->read_ring);
    if(space == 0) return;

    //Need to do immediate reads, so cache the timeout and set to instant
    timeout = com->child->in.get_timeout(com->child->user_data);
    com->child->in.set_timeout(0, com->child->user_data);

    //Read filling write index up to either capacity or the read index.
    start_index = ring_index(&com->read_ring, com->read_ring.w_index);
    start_len = com->read_ring.capacity - start_index;
    if(space < start_len) start_len = space; //Can't reach the end, the read index is in front of it

    read_len = com->child->in.read(start_len, com->read_ring.data + start_index, com->child->user_data);
    
    //Update state variables
    com->read_ring.w_index += read_len;
    space -= read_len;

    //Check if more to read
    if(read_len == start_len && space != 0) {
        //Read filling start of buffer up to the read index.
        //More to read possibly, fill from the start of the buffer now
        end_index = ring_index(&com->read_ring, com->read_ring.r_index);
        read_len = com->child->in.read(end_index, com->read_ring.data, com->child->user_data);

        com->read_ring.w_index += read_len;
    }

    //Restore timeout
    com->child->in.set_timeout(timeout, com->child->user_data);
}

static size_t length(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    fill_in_buffer(com);
    return ring_size(&com->read_ring);
}

static size_t peek_capacity(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->read_ring.capacity;
}
#else
//---------------------------Just basic wrapping-------------------------
static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->in.read(num_bytes, out, com->child->user_data);
}
static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->in.read_until(value, out, size, com->child->user_data);
}
#endif

//--------------------------DIRECT WRAPPING-------------------------------
static int open(void *user_data)
{ 
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->open(com->child->user_data);
}

static int close(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->close(com->child->user_data);
}

static void set_timeout(uint32_t timeout_ms, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->in.set_timeout(timeout_ms, com->child->user_data);
}

uint32_t get_timeout(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->in.get_timeout(com->child->user_data);
}

static void clear_immediate_wrap(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    ring_clear(&com->read_ring);
    com->child->in.clear_immediate(com->child->user_data);
}

static void clear_timeout_wrap(void *user_data, uint32_t timeout_ms)
{
    struct TSS_Ring_Com_Class *com = user_data;
    ring_clear(&com->read_ring);
    com->child->in.clear_timeout(com->child->user_data, timeout_ms);
}

//---------------------------IMPLEMENTATIONS OF CLEARING------------------------------

static void clear_immediate(void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    int8_t buffer[40];
    uint32_t timeout;
    uint8_t len;

    ring_clear(&com->read_ring);

    timeout = com->child->in.get_timeout(com->child->user_data);
    com->child->in.set_timeout(0, com->child->user_data);
    do {
        len = com->child->in.read(sizeof(buffer), buffer, com->child->user_data);
    } while(len > 0);  
    com->child->in.set_timeout(timeout, com->child->user_data);
}

//Implementation of clear timeout using just base functionality
static void clear_timeout(void *user_data, uint32_t timeout_ms)
{
    struct TSS_Ring_Com_Class *com = user_data;
    uint8_t len, buffer[40];
    tss_time_t start, interval_start;
    uint32_t cached_timeout;

    ring_clear(&com->read_ring);

    cached_timeout = com->child->in.get_timeout(com->child->user_data);
    com->child->in.set_timeout(0, com->child->user_data);

    start = tssTimeGet();
    interval_start = start;
    do {
        len = com->child->in.read(sizeof(buffer), buffer, com->child->user_data);
        if(len > 0) {
            interval_start = tssTimeGet();
        } 
    } while(tssTimeDiff(interval_start) < timeout_ms && tssTimeDiff(start) < cached_timeout);

    com->child->in.set_timeout(cached_timeout, com->child->user_data);
}

static int auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *user_data)
{
    struct TSS_Ring_Com_Class *com = user_data;
    return com->child->auto_detect(out, cb, user_data);
}