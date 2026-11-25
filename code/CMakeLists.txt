set(SPONZA_SOURCES
    main.cpp
    system/d3dutils.h
    system/d3dutils.cpp
    system/log.h
    system/log.cpp)

set(SPONZA_SHADERS
    shaders/shader.hlsl)

# Set shader files to "HEADER_ONLY" so that visual studio does not try to compile them
set_source_files_properties(${SPONZA_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Copy shaders to bin/shaders so they can be loaded at runtime relative to the working directory
set(SPONZA_SHADER_OUTPUTS "")
foreach(shader IN LISTS SPONZA_SHADERS)
    set(shaderSrc "${CMAKE_CURRENT_SOURCE_DIR}/${shader}")
    set(shaderDst "${SPONZA_BIN_DIR}/${shader}")
    add_custom_command(
        OUTPUT "${shaderDst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${shaderSrc}" "${shaderDst}"
        DEPENDS "${shaderSrc}"
        COMMENT "Copying ${shader}")
    list(APPEND SPONZA_SHADER_OUTPUTS "${shaderDst}")
endforeach()

add_custom_target(sponza_dx12_shaders DEPENDS ${SPONZA_SHADER_OUTPUTS})

add_executable(sponza_dx12 WIN32 ${SPONZA_SOURCES} ${SPONZA_SHADERS})
add_dependencies(sponza_dx12 sponza_dx12_shaders)

# Mirror the files on disk in the Solution Explorer view
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SPONZA_SOURCES} ${SPONZA_SHADERS})

target_include_directories(sponza_dx12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(sponza_dx12 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    # TODO(sbalse): Do we need these UNICODE defines? Can we directly use either the A or W variants of Win32 functions everywhere?
    UNICODE
    _UNICODE
    $<$<CONFIG:debug>:SPONZA_DEBUG>
    $<$<CONFIG:release>:SPONZA_RELEASE>)

target_link_libraries(sponza_dx12 PRIVATE
    d3d12 dxgi dxguid d3dcompiler)

set_target_properties(sponza_dx12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SPONZA_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SPONZA_BIN_DIR}"
    OUTPUT_NAME_DEBUG "sponza_dx12_d"
    OUTPUT_NAME_RELEASE "sponza_dx12_r"
    VS_DEBUGGER_WORKING_DIRECTORY "${SPONZA_BIN_DIR}")

set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT sponza_dx12)
