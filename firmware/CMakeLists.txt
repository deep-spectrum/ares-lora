cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lora-start)

include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)

include_directories(include)

set(SRC
        src/main.c
        src/settings.c
        src/lora_handlers.c
)

set(SERIAL_SRC
        src/serial/frame.c
        src/serial/serial.c
        src/serial/serial_backend.c
)

set(LORA_SRC
        src/lora/lora.c
        src/lora/lora_backend.c
        src/lora/packet.c
)

set(BLE_SRC
        src/ble/ble.c
        src/ble/services/ares_service.c
)

set(SHELL_SRC
        src/shell/ble.c
)

target_sources(app PRIVATE ${SRC} ${SERIAL_SRC} ${LORA_SRC} ${BLE_SRC})
target_sources_ifdef(CONFIG_SHELL app PRIVATE ${SHELL_SRC})
target_sources_ifndef(CONFIG_SHELL app PRIVATE src/serial_commands.c)

zephyr_iterable_section(NAME ares_serial_command
        KVMA RAM_REGION GROUP RODATA_REGION
        SUBALIGN Z_LINK_ITERABLE_SUBALIGN)
zephyr_linker_sources(SECTIONS linker/ares_iterables_rom.ld)
