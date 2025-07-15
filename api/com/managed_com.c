#include "tss/com/managed_com.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

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

static int reenumerate(TssComAutoDetectCallback cb, void *detect_data, void *user_data);

int tssCreateManagedCom(struct TSS_Com_Class *child, uint8_t *read_buf, size_t read_size, 
    uint8_t *write_buf, size_t write_size, struct TSS_Managed_Com_Class *out)
{
    if(!TSS_RING_POW_2(read_size)) {
        return TSS_ERR_INVALID_SIZE;
    }

    //Add any missing functions to the child com class.
    tssManagedComAddDefaults(child);

    *out = (struct TSS_Managed_Com_Class) {
        //Attributes
        .child = child,
        .read_ring = {
            .capacity = read_size,
            .data = read_buf
        },
        .write_buffer = write_buf,
        .write_buffer_size = write_size,

        //Functions
        .base = {
            .open = open,
            .close = close,
            .reenumerate = (child->reenumerate) ? reenumerate : NULL,
            .auto_detect = (child->auto_detect) ? child->auto_detect : NULL,
            .user_data = out,
            .in = {
                .set_timeout = set_timeout,
                .get_timeout = get_timeout,
                .clear_immediate = clear_immediate,
                .clear_timeout = clear_timeout,
                .read = read,
                .read_until = read_until,
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

    return TSS_SUCCESS;
}

void tssManagedComAddDefaults(struct TSS_Com_Class *com)
{
    if(com->in.read_until == NULL) {
        com->in.read_until = tssManagedComBaseReadUntil;
    }

    if(com->in.clear_immediate == NULL) {
        com->in.clear_immediate = tssManagedComBaseClear;
    }

    if(com->in.clear_timeout == NULL) {
        com->in.clear_timeout = tssManagedComBaseClearTimeout;
    }
}

//--------------------------CUSTOM WRITE BEHAVIOR----------------------------------
#if TSS_BUFFERED_WRITES
static int begin_write(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    com->write_buffer_index = 0;
    return TSS_SUCCESS;
}

static int end_write(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    return com->child->out.write(com->write_buffer, com->write_buffer_index, com->child->user_data);
}
#endif

static int write(const uint8_t *bytes, size_t len, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
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
    struct TSS_Managed_Com_Class *com = user_data;
    size_t peek_len, i;
    peek_len = ring_size(&com->read_ring);
    for(i = 0; i < peek_len && i < num_bytes; i++) {
        out[i] = ring_pop(&com->read_ring);
    }

    return i + com->child->in.read(num_bytes - i, out + i, com->child->user_data);   
}

static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
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

static int peek(size_t start, size_t num_bytes, uint8_t *out, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
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
    struct TSS_Managed_Com_Class *com = user_data;
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

inline static void fill_in_buffer(struct TSS_Managed_Com_Class *com)
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
    struct TSS_Managed_Com_Class *com = user_data;
    fill_in_buffer(com);
    return ring_size(&com->read_ring);
}

static size_t peek_capacity(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    return com->read_ring.capacity;
}
#else
//---------------------------Just basic wrapping-------------------------
static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    return com->child->in.read(num_bytes, out, com->child->user_data);
}
static int read_until(uint8_t value, uint8_t *out, size_t size, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    return com->child->in.read_until(value, out, size, com->child->user_data);
}
#endif

//--------------------------DIRECT WRAPPING-------------------------------

static int open(void *user_data)
{ 
    struct TSS_Managed_Com_Class *com = user_data;
    return com->child->open(com->child->user_data);
}

static int close(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    com->write_buffer_index = 0;
    ring_clear(&com->read_ring);
    return com->child->close(com->child->user_data);
}

static void set_timeout(uint32_t timeout_ms, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    com->child->in.set_timeout(timeout_ms, com->child->user_data);
}

uint32_t get_timeout(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    return com->child->in.get_timeout(com->child->user_data);
}

static void clear_immediate(void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;
    ring_clear(&com->read_ring);
    com->child->in.clear_immediate(com->child->user_data);
}

static void clear_timeout(void *user_data, uint32_t timeout_ms)
{
    struct TSS_Managed_Com_Class *com = user_data;
    ring_clear(&com->read_ring);
    com->child->in.clear_timeout(com->child->user_data, timeout_ms);
}

//--------------------------------------DISCOVERY---------------------------------------------

struct PortEnumerate {
    struct TSS_Managed_Com_Class *self;
    TssComAutoDetectCallback cb;
    void *detect_data;
};


//When reenumerating, need to wrap the child class with this type before sending to the actual callback
static int reenumerate_callback(struct TSS_Com_Class *com, void *user_data)
{
    struct PortEnumerate *info = user_data;

    struct TSS_Managed_Com_Class *managed = info->self;
    if(&managed->base != com) {
        //In the case where the TSS_Managed_Com_Class is included in the struct of the child com class
        //we don't want to wrap the managed class with itself. The child class reenumerate should have already handled the wrapping
        //and therefore this should only be done if com != managed
        tssCreateManagedCom(com, managed->read_ring.data, managed->read_ring.capacity, managed->write_buffer, managed->write_buffer_size, managed);
    }

    return info->cb(&managed->base, info->detect_data);
}

static int reenumerate(TssComAutoDetectCallback cb, void *detect_data, void *user_data)
{
    struct TSS_Managed_Com_Class *com = user_data;

    struct PortEnumerate info = {
        .self = user_data,
        .cb = cb,
        .detect_data = detect_data
    };

    //Detect based on the actual child type since this is just a wrapper
    return com->child->reenumerate(reenumerate_callback, &info, com->child->user_data);
}

//------------------------------------BASE FUNCTION VERSIONS--------------------------------------

int tssManagedComBaseReadUntil(uint8_t value, uint8_t *out, size_t size, void *com_class)
{
    struct TSS_Com_Class *com = com_class;
    size_t num_read;
    tss_time_t start_time;
    uint32_t timeout;

    timeout = get_timeout(com);
    start_time = tssTimeGet();
    num_read = 0;

    //Want to be able to poll instantly, will change back after
    com->in.set_timeout(0, com->user_data);
    while(num_read < size && tssTimeDiff(start_time) < timeout) {
        if(com->in.read(1, out, com->user_data)) {
            num_read++;
            if(*out == value) {
                break;
            }
            out++;
        }
    }

    //Restore timeout back to what it was
    com->in.set_timeout(timeout, com->user_data);
    return num_read;
}

void tssManagedComBaseClear(void *com_class)
{
    struct TSS_Com_Class *com = com_class;
    int8_t buffer[40];
    uint32_t timeout;
    uint8_t len;

    timeout = com->in.get_timeout(com->user_data);
    com->in.set_timeout(0, com->user_data);
    do {
        len = com->in.read(sizeof(buffer), buffer, com->user_data);
    } while(len > 0);  
    com->in.set_timeout(timeout, com->user_data);
}

void tssManagedComBaseClearTimeout(void *com_class, uint32_t timeout_ms)
{
    struct TSS_Com_Class *com = com_class;
    uint8_t len, buffer[40];
    tss_time_t start, interval_start;
    uint32_t cached_timeout;

    cached_timeout = com->in.get_timeout(com->user_data);
    com->in.set_timeout(0, com->user_data);

    start = tssTimeGet();
    interval_start = start;
    do {
        len = com->in.read(sizeof(buffer), buffer, com->user_data);
        if(len > 0) {
            interval_start = tssTimeGet();
        } 
    } while(tssTimeDiff(interval_start) < timeout_ms && tssTimeDiff(start) < cached_timeout);

    com->in.set_timeout(cached_timeout, com->user_data);
}