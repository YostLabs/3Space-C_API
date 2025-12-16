#include "tss/com/serial.h"
#include "tss/errors.h"
#include "tss/sys/time.h"

#include <stdbool.h>

static int open(struct TSS_Com_Class *com);
static int close(struct TSS_Com_Class *com);

static int read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out);

static void set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms);
uint32_t get_timeout(struct TSS_Com_Class *com);

static int write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

static int reenumerate(struct TSS_Com_Class *com, TssComAutoDetectCallback cb, void *detect_data);

const static struct TSS_Com_Class_API m_serial_com_api = {
    .open = open,
    .close = close,

    .reenumerate = reenumerate,
    .auto_detect = serial_com_auto_detect,

    .in = {
        .read = read,
        .read_until = tssManagedComBaseReadUntil,

        .set_timeout = set_timeout,
        .get_timeout = get_timeout,

        .clear_immediate = tssManagedComBaseClear,
        .clear_timeout = tssManagedComBaseClearTimeout
    },
    .out = {
        .write = write
    },
};

void create_serial_com_class(uint8_t port, struct SerialComClass *out)
{
    *out = (struct SerialComClass) {
        .port = {
            .port = port,
        },
        .serial_com = (struct TSS_Com_Class) {
            .api = &m_serial_com_api,
            .reenumerates = true,
        },
    };

    //Wrap it in the default functions
    tssCreateManagedCom(&out->serial_com, (struct TSS_Com_Class*)out, out->read_buffer, sizeof(out->read_buffer), out->write_buffer, sizeof(out->write_buffer), &out->base);
}

static int write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    serWrite(&self->port, (char*)bytes, len);
    return 0;
}

static int open(struct TSS_Com_Class *com)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    int result = serOpen(self->port.port, 115200, &self->port);
    if(result) return result;
    serConfigBufferSize(&self->port, 4096, 64);
    return 0;
}

static int close(struct TSS_Com_Class *com)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    serClose(&self->port);

    return 0;
}

static int read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    return serRead(&self->port, (char*)out, num_bytes);
}

static void set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    serSetTimeout(&self->port, timeout_ms);
}

uint32_t get_timeout(struct TSS_Com_Class *com)
{
    struct SerialComClass *self = (struct SerialComClass *)com;
    return self->port.timeout;
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

static int reenumerate(struct TSS_Com_Class *com, TssComAutoDetectCallback cb, void *detect_data)
{
    return serial_com_auto_detect(com, cb, detect_data);
}