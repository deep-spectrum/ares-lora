cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lora-start)

include_directories(include)

set(SRC
        src/main.c
        src/settings.c
        src/serial_commands.c
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

target_sources(app PRIVATE ${SRC} ${SERIAL_SRC} ${LORA_SRC})
