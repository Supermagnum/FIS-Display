# Display test firmware: cycles through all FIS display modes every 4 seconds.
# Reuses firmware/ for 3LB RX/TX, display, nav_state, config, time/tz.
# No serial parser; demo state is generated internally.

cmake_minimum_required(VERSION 3.13)

# Reuse SDK import from main firmware (or set PICO_SDK_PATH).
include(${CMAKE_CURRENT_SOURCE_DIR}/../firmware/pico_sdk_import.cmake)

project(fis_display_test C CXX ASM)

pico_sdk_init()

set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../firmware)

add_executable(fis_display_test
    display_test_main.c
    ${FIRMWARE_DIR}/fis_display.c
    ${FIRMWARE_DIR}/fis_rx.c
    ${FIRMWARE_DIR}/fis_config.c
    ${FIRMWARE_DIR}/local_time.c
    ${FIRMWARE_DIR}/tz_table.c
    ${FIRMWARE_DIR}/tz_lookup.c
)

pico_generate_pio_header(fis_display_test ${FIRMWARE_DIR}/fis_3lb_rx.pio)
pico_generate_pio_header(fis_display_test ${FIRMWARE_DIR}/fis_3lb_tx.pio)

target_include_directories(fis_display_test PRIVATE ${FIRMWARE_DIR})

target_link_libraries(fis_display_test
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_gpio
    hardware_sync
)

pico_enable_stdio_usb(fis_display_test 0)
pico_enable_stdio_uart(fis_display_test 0)

pico_add_extra_outputs(fis_display_test)
