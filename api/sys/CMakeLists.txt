target_sources(TSS_Api
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/stdinc.c
        ${CMAKE_CURRENT_LIST_DIR}/endian.c
        ${CMAKE_CURRENT_LIST_DIR}/time.c
)

# clock_gettime (used in time.c) requires _POSIX_C_SOURCE >= 199309L on Linux
target_compile_definitions(TSS_Api PRIVATE
    $<$<PLATFORM_ID:Linux>:_POSIX_C_SOURCE=199309L>
)