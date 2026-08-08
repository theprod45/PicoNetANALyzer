cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(PicoNetANALyzer C CXX ASM)

pico_sdk_init()

add_executable(PicoNetANALyzer
    main.c
    network_events.c
    telemetry.c
)

target_include_directories(PicoNetANALyzer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(PicoNetANALyzer
    pico_stdlib
    hardware_uart
)

pico_enable_stdio_usb(PicoNetANALyzer 1)
pico_enable_stdio_uart(PicoNetANALyzer 0)

pico_add_extra_outputs(PicoNetANALyzer)
