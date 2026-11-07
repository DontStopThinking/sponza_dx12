set(SPONZA_SOURCES
    main.cpp)

add_executable(sponza_dx12 ${SPONZA_SOURCES})

# Mirror the files on disk in the Solution Explorer view
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SPONZA_SOURCES})

target_include_directories(sponza_dx12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(sponza_dx12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SPONZA_BIN_DIR}"
    OUTPUT_NAME_DEBUG "sponza_dx12_d"
    OUTPUT_NAME_RELEASE "sponza_dx12_r"
    VS_DEBUGGER_WORKING_DIRECTORY "${SPONZA_BIN_DIR}")

set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT sponza_dx12)
