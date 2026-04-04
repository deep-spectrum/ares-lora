cmake_minimum_required(VERSION 3.15)
project(ares_data_structures)

set(CMAKE_CXX_STANDARD 17)

# If sources are needed in the future, switch to STATIC <source files>
add_library(ares_data_structures INTERFACE)
# Uncomment if sources are included in the build
# set_target_properties(ares_data_structures PROPERTIES POSITION_INDEPENDENT_CODE ON)
# If sources are added, switch INTERFACE to PUBLIC
target_include_directories(ares_data_structures INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
