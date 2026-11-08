set(SPONZA_SOURCES
    main.cpp
    app/app.h
    app/app.cpp
    rhi/rhi.h
    rhi/rhi.cpp
    system/log.h
    system/log.cpp)

add_executable(sponza_dx12 WIN32 ${SPONZA_SOURCES})

# Mirror the files on disk in the Solution Explorer view
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SPONZA_SOURCES})

target_include_directories(sponza_dx12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(sponza_dx12 PRIVATE
    WIN32_LEAN_AND_MEAN
    # TODO(sbalse): Do we need these UNICODE defines? Can we directly use either the A or W variants of Win32 functions everywhere?
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:SPONZA_DEBUG=1>)

target_link_libraries(sponza_dx12 PRIVATE
    d3d12 dxgi)

set_target_properties(sponza_dx12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SPONZA_BIN_DIR}"
    OUTPUT_NAME_DEBUG "sponza_dx12_d"
    OUTPUT_NAME_RELEASE "sponza_dx12_r"
    VS_DEBUGGER_WORKING_DIRECTORY "${SPONZA_BIN_DIR}")

set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT sponza_dx12)
