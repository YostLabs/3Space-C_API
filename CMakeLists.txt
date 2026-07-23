cmake_minimum_required(VERSION 3.12.0)
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables set by set() in parent scope
project(TSS_Api VERSION 0.1.0 LANGUAGES C CXX)

# Build the API Library
add_library(TSS_Api SHARED)
add_subdirectory(api)
target_compile_definitions(TSS_Api PRIVATE TSS_API_EXPORTS)
target_include_directories(TSS_Api PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Build the Communication Class Libraries
# Each library owns its sources, platform backend, includes, and external deps.
# Enable optional com classes that have external dependencies here.
set(TSS_COM_LINUX_SPI ON)  # ON requires libgpiod (sudo apt install libgpiod-dev) when on Linux
add_subdirectory(communication)

# Build the Executable
# All required includes and libraries are propagated automatically.
# Change which com classes get compiled in at the target_link_libraries() call below.
add_executable(my_example
    ${PROJECT_SOURCE_DIR}/Examples/linux_spi_example.c
    #${PROJECT_SOURCE_DIR}/Examples/sending_commands.c
    #${PROJECT_SOURCE_DIR}/Examples/firmware_upload.c
)
target_compile_definitions(my_example PRIVATE _CRT_SECURE_NO_WARNINGS)

# An application is explicitly a combination of the API + one or more com classes.
# Swap or add targets here (tss_linux_serial, tss_linux_spi, tss_win_serial, ...).
target_link_libraries(my_example PRIVATE TSS_Api tss_linux_spi)