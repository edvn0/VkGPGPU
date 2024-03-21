cmake_minimum_required(VERSION 3.21)
project(ShaderCompilation)

set(SOURCES
    include/compilation/ShaderCompiler.hpp
    src/compilation/ShaderCompiler.cpp
    include/reflection/ReflectionData.hpp
    include/reflection/Reflector.hpp
    src/reflection/Reflector.cpp
)
add_library(ShaderCompilation STATIC ${SOURCES})
target_link_libraries(ShaderCompilation fmt::fmt Vulkan::Vulkan spirv-cross-core spirv-cross-glsl Vulkan::shaderc_combined)
target_include_directories(ShaderCompilation PUBLIC
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Core/inline
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm)
