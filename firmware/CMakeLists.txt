cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_fis_bridge C CXX ASM)

pico_sdk_init()

add_executable(pico_fis_bridge
    fis_bridge.c
    fis_config.c
    fis_rx.c
    fis_display.c
    serial_parser.c
    tz_table.c
    tz_lookup.c
    local_time.c
)

pico_generate_pio_header(pico_fis_bridge ${CMAKE_CURRENT_LIST_DIR}/fis_3lb_rx.pio)
pico_generate_pio_header(pico_fis_bridge ${CMAKE_CURRENT_LIST_DIR}/fis_3lb_tx.pio)

target_link_libraries(pico_fis_bridge
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_gpio
    hardware_dma
    hardware_sync
)

pico_enable_stdio_usb(pico_fis_bridge 1)
pico_enable_stdio_uart(pico_fis_bridge 0)

pico_add_extra_outputs(pico_fis_bridge)

