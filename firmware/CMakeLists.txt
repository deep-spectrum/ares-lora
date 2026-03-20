cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lora-start)

include_directories(include)

set(SRC
        src/main.c
        src/lora.c
        src/lora_frame.c
)

set(SERIAL_SRC
        src/serial/frame.c
)

target_sources(app PRIVATE ${SRC} ${SERIAL_SRC})
