cmake_minimum_required(VERSION 3.21)
project(Reflection)

set(SOURCES
    include/reflection/Reflector.hpp
    src/reflection/Reflector.cpp
)
add_library(Reflection STATIC ${SOURCES})
target_link_libraries(Reflection fmt::fmt Vulkan::Vulkan spirv-cross-core spirv-cross-glsl)
target_include_directories(Reflection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_SOURCE_DIR}/Core/include ${CMAKE_SOURCE_DIR}/Core/inline)
