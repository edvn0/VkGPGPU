cmake_minimum_required(VERSION 3.21)
project(App)

add_executable(App ClientApp.hpp main.cpp ClientApp.cpp widgets/Widget.hpp widgets/include/FilesystemWidget.hpp widgets/src/FilesystemWidget.cpp)
target_link_libraries(App PRIVATE Core fmt::fmt Vulkan::Vulkan)
target_include_directories(App PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets)
target_sources(App PRIVATE version.rc)

file(GLOB_RECURSE VERTEX_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
file(GLOB_RECURSE FRAGMENT_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
file(GLOB_RECURSE COMPUTE_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")

set(SHADER_SOURCES ${VERTEX_SHADERS} ${FRAGMENT_SHADERS} ${COMPUTE_SHADERS})

foreach(SHADER IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )
    list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(App CompileShaders)
