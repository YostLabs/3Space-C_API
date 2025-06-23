#include "tss/com/serial.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

#include <stdbool.h>

static int open(void *user_data);
static int close(void *user_data);

static int read(size_t num_bytes, uint8_t *out, void *user_data);

static void set_timeout(uint32_t timeout_ms, void *user_data);
uint32_t get_timeout(void *user_data);

static int write(const uint8_t *bytes, size_t len, void *user_data);

static int reenumerate(TssComAutoDetectCallback cb, void *detect_data, void *user_data);

void create_serial_com_class(uint8_t port, struct SerialComClass *out)
{
    *out = (struct SerialComClass) {
        .port = {
            .port = port,
        },
        .serial_com = (struct TSS_Com_Class) {
            .user_data = out,
            
            .open = open,
            .close = close,

            .reenumerate = reenumerate,
            .auto_detect = serial_com_auto_detect,
            
            .in = {
                .read = read,

                .set_timeout = set_timeout,
                .get_timeout = get_timeout,
            },
            .out = {
                .write = write
            },
        },
    };

    //Wrap it in the default functions
    tssCreateManagedCom(&out->serial_com, out->read_buffer, sizeof(out->read_buffer), out->write_buffer, sizeof(out->write_buffer), &out->base);
}

static int write(const uint8_t *bytes, size_t len, void *user_data)
{
    struct SerialComClass *com = user_data;
    serWrite(&com->port, (char*)bytes, len);
    return 0;
}

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

static int read(size_t num_bytes, uint8_t *out, void *user_data)
{
    struct SerialComClass *com = user_data;
    return serRead(&com->port, (char*)out, num_bytes);
}

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

struct PortEnumerate {
    struct TSS_Com_Class *out;
    TssComAutoDetectCallback cb;
    void *detect_data;
    int result;
};

static uint8_t auto_detect(const char *name, uint8_t port, void *detect_data)
{
    struct PortEnumerate *params = detect_data;

    //TSS_Com_Class is the first element of  SerialComClass, so this is a valid cast
    struct SerialComClass *com = (struct SerialComClass*) params->out;
    create_serial_com_class(port, com);

    if(params->cb != NULL) {
        params->result = params->cb((struct TSS_Com_Class*)com, params->detect_data);
    }
    else {
        params->result = TSS_AUTO_DETECT_SUCCESS;
    }
    
    if(params->result != TSS_AUTO_DETECT_CONTINUE) {
        return SER_ENUM_STOP;
    }

    return SER_ENUM_CONTINUE;
}

int serial_com_auto_detect(struct TSS_Com_Class *out, TssComAutoDetectCallback cb, void *detect_data)
{
    //The serEnumerate is also implemented as a callback function, so have to wrap it
    struct PortEnumerate params = {
        .out = out,
        .cb = cb,
        .detect_data = detect_data,
        .result = TSS_AUTO_DETECT_CONTINUE
    };

    serEnumeratePorts(auto_detect, &params);
    if(params.result == TSS_AUTO_DETECT_CONTINUE) {
        return TSS_AUTO_DETECT_DONE;
    }
    return params.result;
}

static int reenumerate(TssComAutoDetectCallback cb, void *detect_data, void *user_data)
{
    struct SerialComClass *com = user_data;
    return serial_com_auto_detect((struct TSS_Com_Class*)com, cb, detect_data);
}