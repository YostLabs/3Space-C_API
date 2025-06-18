#if defined(_WIN32) || defined(_WIN64)
#include "tss/com/backend/serial/ser_device.h"
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>


int serOpen(uint8_t port, uint32_t baudrate, struct SerialDevice *out)
{
    #define COM_MAXNAME    12
    char name[COM_MAXNAME];
    HANDLE handle;
    COMMTIMEOUTS timeouts;
    DCB config;

    *out = (struct SerialDevice) {
        .port = port,
        .timeout = 1000 //Default
    };

    serPortToName(port, name, COM_MAXNAME);

    //Open the port
    handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(handle == INVALID_HANDLE_VALUE) {
        printf("Failed to open serial port %s with error %d.\n", name, GetLastError());
        return -1;
    };
    out->handle = handle;

    //Initialize the port
    SetupComm(handle, 64, 64);

    //Handle base configuration
    GetCommState(handle, &config);
    config.BaudRate = baudrate;
    config.Parity = 0;
    config.fAbortOnError = 0;
    config.ByteSize = 8;

    if(SetCommState(handle, &config) == 0) {
        CloseHandle(handle);
        return -1;
    }

    //Initialize timeout
    GetCommTimeouts(handle, &timeouts);
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = out->timeout;
    SetCommTimeouts(handle, &timeouts);

    return 0;
}

void serClose(struct SerialDevice *ser)
{
    if(!ser->handle) {
        return;
    }
    CloseHandle(ser->handle);
    ser->handle = 0;
}

int serConfigBufferSize(struct SerialDevice *ser, uint32_t in_size, uint32_t out_size)
{
    return !SetupComm(ser->handle, in_size, out_size);
}

uint32_t serRead(struct SerialDevice *ser, char *buffer, uint32_t len)
{
    DWORD num_read = 0;
    bool success = ReadFile(ser->handle, buffer, len, &num_read, NULL);
    if(!success) {
        printf("Failed Read %d\r\n", GetLastError());
    }
    return num_read;
}

uint32_t serReadImmediate(struct SerialDevice *ser, char *buffer, uint32_t len)
{
    COMMTIMEOUTS timeouts;
    COMMTIMEOUTS cached_timeouts;
    GetCommTimeouts(ser->handle, &timeouts);
    cached_timeouts = timeouts;

    //This causes it not to block
    //https://learn.microsoft.com/en-us/windows/win32/devio/time-outs
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(ser->handle, &timeouts);

    uint32_t result = serRead(ser, buffer, len);

    //Revert the timeouts back for continued use
    SetCommTimeouts(ser->handle, &cached_timeouts);

    return result;
}

uint32_t serWrite(struct SerialDevice *ser, const char *buffer, uint32_t len)
{
    DWORD num_bytes = 0;
    WriteFile(ser->handle, buffer, len, &num_bytes, NULL);
    return num_bytes;
}

void serClear(struct SerialDevice *ser)
{
    bool result = PurgeComm(ser->handle, PURGE_RXCLEAR);
}

uint32_t serGetTimeout(struct SerialDevice *ser)
{
    COMMTIMEOUTS timeouts;
    GetCommTimeouts(ser->handle, &timeouts);
    return 0;
}

void serSetTimeout(struct SerialDevice *ser, uint32_t timeout_ms)
{
    ser->timeout = timeout_ms;
    
    COMMTIMEOUTS timeouts;
    GetCommTimeouts(ser->handle, &timeouts);
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    SetCommTimeouts(ser->handle, &timeouts);
}

const char * serPortToName(uint8_t port, char *out, size_t size)
{
    snprintf(out, size, "//./COM%i", port);
    return out;
}

static const char * findPattern(const char ** string, const char * pattern, int * value)
{
    char c, n = 0;
    const char * start = *string;
    const char * pp = pattern;
    // Check for the string pattern
    while (1) {
        c = *(*string)++;
        if (c == '\0') {
            if (*pp == '?') break;
            if (**string == '\0') break;
            n = 0;
            pp = pattern;
            start = *string;
        }else{
            if (*pp == '?') {
            // Expect a digit
                if (c >= '0' && c <= '9') {
                    n = n * 10 + (c - '0');
                    if (*pp ++ == '\0') break;
                }else{
                    n = 0;
                    pp = pattern;
                }
            }else{
            // Expect a character
                if (c == *pp) {
                    if (*pp ++ == '\0') break;
                }else{
                    n = 0;
                    pp = pattern;
                }
            }
        }
    }
// Return the value
    * value = n;
    return start;
}


uint8_t serEnumeratePorts(uint8_t (*cb)(const char *name, uint8_t port, void *user_data), void *user_data)
{
    size_t size = 4096;

    //Obtain all devices connected. May have to expand this list a few times before 
    //everything can fit
    char * list = (char *) malloc(size);
    SetLastError(0);
    QueryDosDeviceA(NULL, list, (uint32_t) size);
    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        size *= 2;
        char * nlist = realloc(list, size);
        if (!nlist) {
            free(list);
        }
        list = nlist;
        SetLastError(0);
        QueryDosDeviceA(NULL, list, (uint32_t) size);
    }

    //Gather all the com ports from the result
    int port;
    const char *nlist = list;
    const char *portname = findPattern(&nlist, "COM???", &port);
    uint8_t ports_discovered = 0;
    while(port > 0) {
        ports_discovered++;
        if(cb(portname, port, user_data) == SER_ENUM_STOP) {
            break;
        }
        portname = findPattern(&nlist, "COM???", &port);
    }
    free(list);

    return ports_discovered;
}

#endif