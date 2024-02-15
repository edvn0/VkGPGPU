cmake_minimum_required(VERSION 3.21)
project(App)

add_executable(App ClientApp.hpp main.cpp ClientApp.cpp
    widgets/Widget.hpp
    widgets/include/FilesystemWidget.hpp
    widgets/include/SceneWidget.hpp
    widgets/src/FilesystemWidget.cpp
    widgets/src/SceneWidget.cpp
)
target_link_libraries(App PRIVATE Core ECS fmt::fmt Vulkan::Vulkan imguizmo)
target_include_directories(App PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets ${CMAKE_SOURCE_DIR}/ThirdParty/glm)
target_sources(App PRIVATE version.rc)
target_precompile_headers(App REUSE_FROM Core)

if(GPGPU_CUSTOM_BASE_PATH)
    target_compile_definitions(App PRIVATE -DGPGPU_CUSTOM_BASE_PATH="${GPGPU_CUSTOM_BASE_PATH}")
endif()

# Only if Vulkan_GLSLC_EXECUTABLE is set
if(Vulkan_GLSLC_EXECUTABLE)
    file(GLOB_RECURSE VERTEX_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
    file(GLOB_RECURSE FRAGMENT_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
    file(GLOB_RECURSE COMPUTE_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")

    set(SHADER_SOURCES ${VERTEX_SHADERS} ${FRAGMENT_SHADERS} ${COMPUTE_SHADERS})

    foreach(SHADER IN LISTS SHADER_SOURCES)
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SHADER_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_NAME}.spv")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT} -I ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include -g
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${SHADER_NAME}"
        )
        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
    add_dependencies(App CompileShaders)
endif()
