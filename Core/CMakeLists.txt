cmake_minimum_required(VERSION 3.21)
project(Core)

set(SOURCES
    include/Buffer.hpp
    src/Buffer.cpp
    include/Device.hpp
    src/Device.cpp
    include/Instance.hpp
    src/Instance.cpp
    include/Types.hpp
    include/Logger.hpp
    src/Logger.cpp
    include/Verify.hpp
    src/Verify.cpp
    include/Allocator.hpp
    src/Allocator.cpp
    include/Exception.hpp
    include/Filesystem.hpp
    include/Shader.hpp
    src/Shader.cpp
    include/Pipeline.hpp
    src/Pipeline.cpp
    include/CommandBuffer.hpp
    src/CommandBuffer.cpp
    include/DescriptorMap.hpp
    src/DescriptorMap.cpp
    include/DebugMarker.hpp
    src/DebugMarker.cpp
    include/Formatters.hpp
    src/Formatters.cpp
    include/Timer.hpp
    src/Timer.cpp
    include/DynamicLibraryLoader.hpp
    src/DynamicLibraryLoader.cpp
)

add_library(Core STATIC ${SOURCES})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/inline ${CMAKE_SOURCE_DIR}/Platform ${CMAKE_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include)
target_link_libraries(Core PUBLIC Platform Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator fmt::fmt)
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
    target_compile_options(Core PRIVATE -Wno-nullability-completeness)
endif()