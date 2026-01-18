# FLAC Decode Benchmark for ESP32

cmake_minimum_required(VERSION 3.16)

# Get the library component directory and extract its name for REQUIRES
get_filename_component(LIB_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(LIB_COMPONENT_NAME "${LIB_COMPONENT_DIR}" NAME)

# Include the library component
list(APPEND EXTRA_COMPONENT_DIRS "${LIB_COMPONENT_DIR}")

# Pass the component name to subprojects
set(ESP_AUDIO_LIBS_COMPONENT "${LIB_COMPONENT_NAME}" CACHE STRING "esp-audio-libs component name")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(flac_decode_benchmark)
