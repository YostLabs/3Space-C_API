/*
 * Example custom COM class implementation.
 * This shows the bare minimum that needs to be done to create a com class
 * that can be used for communication with a device. It utilizes the TSS_Managed_Com_Class
 * to provide default functionality for multiple functions to reduce the workload of creating a custom com class.
 * 
 * There are two primary ways of creating a TSS_Managed_Com_Class. The first is to include the TSS_Managed_Com_Class 
 * as part of the custom com class struct, and then call tssCreateManagedCom() to initialize it. This is the method used in this example.
 * 
 * The second method is to dynamically create the TSS_Managed_Com_Class, and then use tssCreateManagedComDynamic() to initialize it. 
 * The changes necessary to implement this method are shown in the second half of this file, and are documented with "CHANGE:" comments.
 * For additional information on creating a COM class, check out the wiki page: https://github.com/YostLabs/3Space-C_API/wiki/Communication-Class
 */

#include "tss/com/managed_com.h"
#include "tss/errors.h"
#include "tss/sys/time.h"


struct MyCustomComClass {
    // If including struct TSS_Managed_Com_Class it must be first
    struct TSS_Managed_Com_Class base;

    // This is the com class that we are wrapping with the managed com class
    struct TSS_Com_Class my_com;

    // Custom fields
    int port;
    uint32_t timeout_ms;

    // Required buffer for TSS_MINIMAL_SENSOR=0
    // Must have a size that is a power of 2
    uint8_t read_buffer[4096];

    // Required buffer for TSS_BUFFERED_WRITES=1
    uint8_t write_buffer[512];
};

// Prototypes for the custom defined com class functions
static int open(struct TSS_Com_Class *com);
static int close(struct TSS_Com_Class *com);

static int read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out);
static int write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len);

static void set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms);
static uint32_t get_timeout(struct TSS_Com_Class *com);

const static struct TSS_Com_Class_API k_custom_com_api = {
    .open = open,
    .close = close,

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

void create_my_custom_com_class(uint8_t port, struct MyCustomComClass *out)
{
    *out = (struct MyCustomComClass) {
        .port = port,
        .my_com = (struct TSS_Com_Class) {
            .api = &k_custom_com_api,
            .reenumerates = false,
        },
    };

    // Wrap it in the default functions
    tssCreateManagedCom(&out->my_com, (struct TSS_Com_Class*)out, out->read_buffer, 
        sizeof(out->read_buffer), out->write_buffer, sizeof(out->write_buffer), &out->base);
}

static int open(struct TSS_Com_Class *com)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    // TODO: Open the communication port with appropriate settings
    // Example: int result = com_open(self->port, 115200, &self->port);
    // Example: configBufferSize(self->port, 4096, 64);
    return 0;
}

static int close(struct TSS_Com_Class *com)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    // TODO: Close the communication port
    // Example: com_close(self->port);
    return 0;
}

static int read(struct TSS_Com_Class *com, size_t num_bytes, uint8_t *out)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    // TODO: Read bytes from the communication port
    int num_read = 0;
    // Example: num_read = com_read(self->port, (char*)out, num_bytes);
    return num_read;
}

static int write(struct TSS_Com_Class *com, const uint8_t *bytes, size_t len)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    // TODO: Send bytes to the communication port
    return 0;
}

static void set_timeout(struct TSS_Com_Class *com, uint32_t timeout_ms)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    self->timeout_ms = timeout_ms;
}

static uint32_t get_timeout(struct TSS_Com_Class *com)
{
    struct MyCustomComClass *self = (struct MyCustomComClass *)com;
    return self->timeout_ms;
}

int main() {
    struct MyCustomComClass my_custom_com;
    struct TSS_Com_Class *com_class;

    create_my_custom_com_class(1, &my_custom_com);
    
    // Need to frequently cast to the base class to use the API, easier to store
    // then to cast every time.
    com_class = (struct TSS_Com_Class *)&my_custom_com;
    
    // Use com_class to communicate with the device
    tss_com_open(com_class);
    tss_com_close(com_class);

    return 0;
}

//---------------------------------------DYNAMIC COM CLASS IMPLEMENTATION---------------------------------------------
// This section shows the functions above that would need to be changed, and how, to dynamically creating a managed com class
// instead of storing the TSS_Managed_Com_Class directly as part of the custom com class.
// All necessary changes made are documented by "CHANGE:" comments in the code below.

// struct MyCustomComClass {
//     // CHANGE: Removed struct TSS_Managed_Com_Class base; as the base object
//     // and changed it to the flat com class that is being wrapped.
//     struct TSS_Com_Class my_com;

//     int port;
//     uint32_t timeout_ms;

//     uint8_t read_buffer[4096];

//     uint8_t write_buffer[512];
// };

// void create_my_custom_com_class(uint8_t port, struct MyCustomComClass *out)
// {
//     *out = (struct MyCustomComClass) {
//         .port = port,
//         .my_com = (struct TSS_Com_Class) {
//             .api = &k_custom_com_api,
//             .reenumerates = false,
//         },
//     };

//     // CHANGE: Removed the tssCreatedManagedCom call since it is no longer part 
//     // of the com class directly. The user will instead wrap the com class manually in main
// }

// int main() {
//     struct MyCustomComClass my_custom_com;
//     // CHANGE: Added the Managed Com directly here
//     struct TSS_Managed_Com_Class managed_com;
//     struct TSS_Com_Class *com_class;

//     create_my_custom_com_class(1, &my_custom_com);
    
//     // CHANGE: This function call is different from the original that was in create_my_custom_com_class
//     // The address of the com class being wrapped is only passed once
//     tssCreateManagedComDynamic((struct TSS_Com_Class*)&my_custom_com, my_custom_com.read_buffer, 
//         sizeof(my_custom_com.read_buffer), my_custom_com.write_buffer, sizeof(my_custom_com.write_buffer), &managed_com);

//     // CHANGE: Storing reference to the wrapping managed_com, not the original com class
//     com_class = (struct TSS_Com_Class *)&managed_com;

//     tss_com_open(com_class);
//     tss_com_close(com_class);

//     return 0;
// }