#if defined(_WIN32) || defined(_WIN64)
#include "tss/com/backend/serial/ser_device.h"
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

static void serSetActualTimeout(struct SerialDevice *ser, uint32_t timeout_ms);

int serOpen(uint8_t port, uint32_t baudrate, struct SerialDevice *out)
{
    #define COM_MAXNAME    12
    char name[COM_MAXNAME];
    HANDLE handle;
    DCB config;

    *out = (struct SerialDevice) {
        .port = port,
        .timeout = 1000, //Default
        .blocking = true
    };

    serPortToName(port, name, COM_MAXNAME);

    //Open the port
    handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if(handle == INVALID_HANDLE_VALUE) {
        printf("Failed to open serial port %s with error %d.\n", name, GetLastError());
        return -1;
    };
    out->handle = handle;

    out->overlap_read.hEvent = CreateEventA(NULL, true, false, NULL);
    out->overlap_write.hEvent = CreateEventA(NULL, 0, 0, NULL);

    //Initialize the port
    SetupComm(handle, 4096, 4096);

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
    serSetActualTimeout(out, out->timeout);
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

//Based on PySerial serialwin32.py
uint32_t serRead(struct SerialDevice *ser, char *buffer, uint32_t len)
{
    if(len == 0) return 0;

    ResetEvent(ser->overlap_read.hEvent);

    if(!ser->blocking) {
        DWORD flags;
        COMSTAT comstat;
        if(!ClearCommError(ser->handle, &flags, &comstat)) {
            return 0;
        }
        if(comstat.cbInQue < len) {
            len = comstat.cbInQue;
            if(len == 0) return 0;
        }
    }

    DWORD num_read;
    bool success = ReadFile(ser->handle, buffer, len, &num_read, &ser->overlap_read);
    if(!success) {
        DWORD err = GetLastError();
        if(err != ERROR_SUCCESS && err != ERROR_IO_PENDING) {
            return 0;
        }
    }
    success = GetOverlappedResult(ser->handle, &ser->overlap_read, &num_read, true);
    if(!success && GetLastError() != ERROR_OPERATION_ABORTED) {
        return 0;
    }
    return num_read;
}

uint32_t serWrite(struct SerialDevice *ser, const char *buffer, uint32_t len)
{
    if(len == 0) return 0;
    DWORD num_bytes = 0;
    bool success = WriteFile(ser->handle, buffer, len, &num_bytes, &ser->overlap_write);
    DWORD errorcode = (success) ? ERROR_SUCCESS : GetLastError();
    switch(errorcode) {
        case ERROR_INVALID_USER_BUFFER:
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OPERATION_ABORTED:
            return 0;
        case ERROR_SUCCESS:
            return num_bytes;
        default:
            return 0;
    }
}

void serClear(struct SerialDevice *ser)
{
    bool result = PurgeComm(ser->handle, PURGE_RXCLEAR);
}

static void serSetActualTimeout(struct SerialDevice *ser, uint32_t timeout_ms)
{
    COMMTIMEOUTS timeouts;
    GetCommTimeouts(ser->handle, &timeouts);
    if(timeout_ms == 0) {
        //Timeout of 0 means return instantly. That is configured
        //by setting this to MAXDWORD and the Multiplier and Constant to 0
        timeouts.ReadIntervalTimeout = MAXDWORD;
    }
    else {
        timeouts.ReadIntervalTimeout = 0;
    }
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    SetCommTimeouts(ser->handle, &timeouts);
}

uint32_t serGetTimeout(struct SerialDevice *ser)
{
    if(!ser->blocking) return 0;
    return ser->timeout; 
}

void serSetTimeout(struct SerialDevice *ser, uint32_t timeout_ms)
{
    if(timeout_ms == 0) {
        ser->blocking = false;
        return;
    }
    ser->blocking = true;
    if(timeout_ms == ser->timeout) {
        //This function is REALLY slow, so avoid
        //unecessary calls.
        return;
    }
    ser->timeout = timeout_ms;
    
    serSetActualTimeout(ser, timeout_ms);
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