cmake_minimum_required(VERSION 3.15)
project(${SKBUILD_PROJECT_NAME}
        VERSION ${SKBUILD_PROJECT_VERSION}
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ version selection")  # or 11, 14, 17, 20
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # optional, ensure standard is supported
set(CMAKE_CXX_EXTENSIONS OFF)  # optional, keep compiler extensions off

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/extern/pybind11/pybind11")
    message(FATAL_ERROR "Please run \"git submodule update --init\" before proceeding")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/extern/ares-common/cmake")

add_subdirectory(extern/pybind11)
add_subdirectory(extern/ares-common)

include (AddPythonModule)

include_directories(
        include
)

set(PACKAGE_NAME ares_lora_serial_core)
python_package(src/${PACKAGE_NAME} DESTINATION ${PACKAGE_NAME})
python_module(_core src/core.cpp
        DESTINATION ${PACKAGE_NAME}
        DEFINITIONS VERSION_INFO=${PROJECT_VERSION})
python_module(_ares_lora_serial src/ares_lora_serial/ares_lora_serial.cpp src/ares_lora_serial/ares_frame.cpp
        DESTINATION ${PACKAGE_NAME}
        DEPENDENCIES serial ares_data_structures logging ares_work_q
        LIBS serial ares_data_structures logging ares_work_q)
