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
)

add_library(Core STATIC ${SOURCES})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/inline)
target_link_libraries(Core PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator fmt::fmt)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Core PUBLIC GPGPU_DEBUG)
    target_compile_definitions(Core PUBLIC GPGPU_FRAME_COUNT=12)
endif()
