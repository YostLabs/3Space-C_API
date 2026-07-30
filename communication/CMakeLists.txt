# ---------------------------------------------------------------------------
# Serial Com Libraries
# ---------------------------------------------------------------------------

if(UNIX)
    add_library(tss_linux_serial STATIC EXCLUDE_FROM_ALL
        serial/serial_com_class.c
        serial/linux_serial.c
    )
    target_include_directories(tss_linux_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    # TSS_Api is used internally to compile this library.
    # The consuming application is expected to link TSS_Api directly.
    target_link_libraries(tss_linux_serial PRIVATE TSS_Api tss_warnings)
endif()

if(WIN32)
    add_library(tss_win_serial STATIC EXCLUDE_FROM_ALL
        serial/serial_com_class.c
        serial/win_serial.c
    )
    target_include_directories(tss_win_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(tss_win_serial PRIVATE TSS_Api tss_warnings)
endif()

# ---------------------------------------------------------------------------
# SPI Com Libraries
# ---------------------------------------------------------------------------

# Opt in to SPI support by setting -DTSS_COM_LINUX_SPI=ON (or toggling in cmake-gui).
# This is gated behind an option so that libgpiod is not required on serial-only builds.
# If this is missing, run: sudo apt install libgpiod-dev
option(TSS_COM_LINUX_SPI "Enable the Linux SPI communication class (requires libgpiod)" OFF)

if(TSS_COM_LINUX_SPI AND UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)

    add_library(tss_linux_spi STATIC EXCLUDE_FROM_ALL
        spi/spi_com_class.c
        spi/linux_spi.c
    )
    target_include_directories(tss_linux_spi
        PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${GPIOD_INCLUDE_DIRS}  # only needed to compile this library's sources
    )
    target_link_libraries(tss_linux_spi
        PUBLIC  ${GPIOD_LIBRARIES}  # must propagate: static libs require transitive link deps
        PRIVATE TSS_Api tss_warnings
    )
endif()

# ---------------------------------------------------------------------------
# I2C Com Libraries
# ---------------------------------------------------------------------------
# Opt in to I2C support by setting -DTSS_COM_LINUX_I2C=ON (or toggling in cmake-gui).
# This is gated behind an option so that libgpiod is not required on serial-only builds.
# If this is missing, run: sudo apt install libgpiod-dev
option(TSS_COM_LINUX_I2C "Enable the Linux I2C communication class" OFF)

if(TSS_COM_LINUX_I2C AND UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)

    add_library(tss_linux_i2c STATIC EXCLUDE_FROM_ALL
        i2c/i2c_com_class.c
        i2c/linux_i2c.c
    )
    target_include_directories(tss_linux_i2c 
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${GPIOD_INCLUDE_DIRS}
    )
    target_link_libraries(tss_linux_i2c
        PUBLIC  ${GPIOD_LIBRARIES}
        PRIVATE TSS_Api tss_warnings
    )
endif()