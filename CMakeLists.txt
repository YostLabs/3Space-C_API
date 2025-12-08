cmake_minimum_required(VERSION 3.12.0)
project(TSS_Api VERSION 0.1.0 LANGUAGES C CXX)

#Build the Library
add_library(TSS_Api SHARED)
add_subdirectory(api)
target_compile_definitions(TSS_Api PRIVATE TSS_API_EXPORTS)
target_include_directories(TSS_Api PUBLIC
                ${PROJECT_SOURCE_DIR}/include
)

#Build the Executable
add_executable(my_example 
    ${PROJECT_SOURCE_DIR}/communication/serial/win_serial.c
    ${PROJECT_SOURCE_DIR}/communication/serial/serial_com_class.c

    ${PROJECT_SOURCE_DIR}/Examples/sending_commands.c
    #${PROJECT_SOURCE_DIR}/Examples/firmware_upload.c
    #${PROJECT_SOURCE_DIR}/Examples/ble_test.c
)
target_compile_definitions(my_example PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(my_example PUBLIC
    ${PROJECT_SOURCE_DIR}/communication/include
)

target_link_libraries(my_example PRIVATE TSS_Api)

#BLE Stuff
# target_link_libraries(my_example PRIVATE
#     Setupapi
#     BluetoothApis
# )