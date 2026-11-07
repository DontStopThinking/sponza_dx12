cmake_minimum_required(VERSION 4.3)

if (NOT WIN32)
    message(FATAL_ERROR "This project currently only supports Windows")
endif()

set(CMAKE_CONFIGURATION_TYPES debug release)

project(sponza_dx12 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin>")

set(SPONZA_BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set(SPONZA_EXTERN_DIR "${CMAKE_SOURCE_DIR}/extern")

add_subdirectory(code)
