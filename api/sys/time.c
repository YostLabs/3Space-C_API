#include "tss/sys/time.h"

// Detect Windows
#if defined(_WIN32) || defined(_WIN64)
    #define DEFAULT_IMPLEMENTATION

    #include <windows.h>
    static tss_time_t defaultGetTime(void)
    {   
        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);
        return time.QuadPart;
    }

    //Required to return in milliseconds
    static uint32_t defaultDiffTime(tss_time_t start_time)
    {
        LARGE_INTEGER time;
        LARGE_INTEGER freq;
        QueryPerformanceCounter(&time);
        QueryPerformanceFrequency(&freq);

        return (uint32_t)((((double)(time.QuadPart - start_time)) / freq.QuadPart) * 1000);
    }

// Detect macOS
#elif defined(__APPLE__) && defined(__MACH__)
    #define DEFAULT_IMPLEMENTATION

// Detect Linux
#elif defined(__linux__) || defined(unix)
    #define DEFAULT_IMPLEMENTATION

#endif


#ifdef DEFAULT_IMPLEMENTATION
static tss_time_t (*getTimeFunc)(void) = defaultGetTime;
static uint32_t (*diffTimeFunc)(tss_time_t) = defaultDiffTime;
#else
#include <stddef.h>
static tss_time_t (*getTimeFunc)(void) = NULL;
static uint32_t (*diffTimeFunc)(tss_time_t) = NULL;
#endif

/// @brief Retrieves the current system time
/// @return Time
tss_time_t tssTimeGet(void)
{
    return getTimeFunc();
}

/// @brief Returns the time difference between the current
/// system time and the given start time
/// @param start_time The start time as retrieved by tssTimeGet
/// @return Time difference in milliseconds
uint32_t tssTimeDiff(tss_time_t start_time)
{
    return diffTimeFunc(start_time);
}