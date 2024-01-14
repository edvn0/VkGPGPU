cmake_minimum_required(VERSION 3.21)
project(Core)

set(SOURCES
    include/Allocator.hpp
    include/App.hpp
    include/Buffer.hpp
    include/CommandBuffer.hpp
    include/CommandDispatcher.hpp
    include/Config.hpp
    include/DataBuffer.hpp
    include/DebugMarker.hpp
    include/DescriptorMap.hpp
    include/Device.hpp
    include/DynamicLibraryLoader.hpp
    include/Entry.hpp
    include/Environment.hpp
    include/Exception.hpp
    include/Filesystem.hpp
    include/Formatters.hpp
    include/Image.hpp
    include/Instance.hpp
    include/Logger.hpp
    include/Pipeline.hpp
    include/Shader.hpp
    include/Texture.hpp
    include/Timer.hpp
    include/Types.hpp
    include/Verify.hpp
    include/Window.hpp
    include/pch/vkgpgpu_pch.hpp
    src/Allocator.cpp
    src/App.cpp
    src/Buffer.cpp
    src/CommandBuffer.cpp
    src/DebugMarker.cpp
    src/DescriptorMap.cpp
    src/Device.cpp
    src/DynamicLibraryLoader.cpp
    src/Formatters.cpp
    src/Image.cpp
    src/Instance.cpp
    src/Logger.cpp
    src/Pipeline.cpp
    src/Shader.cpp
    src/Timer.cpp
    src/Texture.cpp
    src/Verify.cpp
    src/Window.cpp
)

add_library(Core STATIC ${SOURCES})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/inline
        ${CMAKE_SOURCE_DIR}/Platform
        ${CMAKE_SOURCE_DIR}/ThirdParty/stb
        ${CMAKE_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include)
target_link_libraries(Core PUBLIC Platform Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator fmt::fmt stbimage::stbimage glfw)
target_precompile_headers(Core PUBLIC include/pch/vkgpgpu_pch.hpp)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Core PUBLIC GPGPU_DEBUG)
    target_compile_definitions(Core PUBLIC GPGPU_FRAME_COUNT=3)
endif()

if(GPGPU_PIPELINE STREQUAL "ON")
    message(STATUS "GPGPU_PIPELINE is ON")
    target_compile_definitions(Core PUBLIC GPGPU_PIPELINE)
else()
    message(STATUS "GPGPU_PIPELINE is OFF")
endif()

# If we're on Linux + clang/gnu, need to set -Wno-nullability-completeness
if(UNIX AND NOT APPLE)
    target_compile_options(Core PRIVATE -Wno-nullability-completeness -Wno-format)
endif()
