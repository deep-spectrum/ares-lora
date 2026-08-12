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

set(ARES_FRAME_SRC
        # BLE
        src/ares_lora_serial/frames/ble/ble_state.cpp
        src/ares_lora_serial/frames/ble/ble_connected.cpp
        src/ares_lora_serial/frames/ble/ble_disconnect.cpp
        src/ares_lora_serial/frames/ble/ble_subscribed.cpp
        src/ares_lora_serial/frames/ble/ble_chunks.cpp
        src/ares_lora_serial/frames/ble/ble_image_chunk.cpp

        # Debug
        src/ares_lora_serial/frames/debug/dbg.cpp
        src/ares_lora_serial/frames/debug/packet_rx.cpp
        src/ares_lora_serial/frames/debug/packet_tx.cpp

        # LoRa
        src/ares_lora_serial/frames/lora/start.cpp
        src/ares_lora_serial/frames/lora/poll/heartbeat.cpp
        src/ares_lora_serial/frames/lora/poll/poll.cpp
        src/ares_lora_serial/frames/lora/log/log.cpp
        src/ares_lora_serial/frames/lora/log/log_ack.cpp
        src/ares_lora_serial/frames/lora/abort.cpp
        src/ares_lora_serial/frames/lora/lora_ack.cpp
        src/ares_lora_serial/frames/lora/config/common.cpp
        src/ares_lora_serial/frames/lora/config/node_config.cpp
        src/ares_lora_serial/frames/lora/config/node_config_poll.cpp
        src/ares_lora_serial/frames/lora/config/node_config_response.cpp
        src/ares_lora_serial/frames/lora/ready.cpp

        # MCU
        src/ares_lora_serial/frames/mcu/setting.cpp
        src/ares_lora_serial/frames/mcu/lora_config.cpp
        src/ares_lora_serial/frames/mcu/led.cpp
        src/ares_lora_serial/frames/mcu/version.cpp
        src/ares_lora_serial/frames/mcu/ack.cpp
        src/ares_lora_serial/frames/mcu/framing_error.cpp
        src/ares_lora_serial/frames/mcu/reboot.cpp

        # Top level
        src/ares_lora_serial/frames/frame.cpp
)

set(ARES_SERIAL_DRIVER_SRC
        # Bindings
        src/ares_lora_serial/serial-driver/bindings.cpp

        # Top level
        src/ares_lora_serial/serial-driver/serial_driver.cpp
        src/ares_lora_serial/serial-driver/frame_dispatcher.cpp
)

set(PACKAGE_NAME ares_lora_serial_core)
python_package(src/${PACKAGE_NAME} DESTINATION ${PACKAGE_NAME})
python_module(_core src/core.cpp
        DESTINATION ${PACKAGE_NAME}
        DEFINITIONS VERSION_INFO=${PROJECT_VERSION})
python_module(_ares_lora_serial ${ARES_SERIAL_DRIVER_SRC} ${ARES_FRAME_SRC}
        DESTINATION ${PACKAGE_NAME}
        DEPENDENCIES serial ares_data_structures logging ares_work_q ares_datetime
        LIBS serial ares_data_structures logging ares_work_q ares_datetime
)
