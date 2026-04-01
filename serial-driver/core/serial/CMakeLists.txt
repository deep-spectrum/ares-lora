cmake_minimum_required(VERSION 3.22)
project(serial)

set(CMAKE_CXX_STANDARD 20)

set(SERIAL_C_API
        src/core/C-API/serial_posix.c
)

set(SERIAL_SRC
        src/core/serial_base.cpp
        src/core/serial_posix.cpp
)

set(SERIAL_TOOLS_CORE
        src/tools/core/sys_fs_common.cpp
        src/tools/core/sys_fs_linux.cpp
)

set(SERIAL_TOOLS
        src/tools/list_ports.cpp
)

add_library(serial STATIC ${SERIAL_C_API} ${SERIAL_SRC} ${SERIAL_TOOLS_CORE} ${SERIAL_TOOLS})
set_target_properties(serial PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
