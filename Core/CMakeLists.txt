cmake_minimum_required(VERSION 3.21)
project(Core)

set(SOURCES
    include/Allocator.hpp
    include/App.hpp
    include/Buffer.hpp
    include/BufferSet.hpp
    include/Camera.hpp
    include/Colours.hpp
    include/CommandBuffer.hpp
    include/CommandDispatcher.hpp
    include/Concepts.hpp
    include/Config.hpp
    include/Containers.hpp
    include/DataBuffer.hpp
    include/DebugMarker.hpp
    include/DescriptorResource.hpp
    include/Device.hpp
    include/DynamicLibraryLoader.hpp
    include/Entry.hpp
    include/Environment.hpp
    include/Event.hpp
    include/Exception.hpp
    include/Filesystem.hpp
    include/Formatters.hpp
    include/Framebuffer.hpp
    include/GIFTexture.hpp
    include/Mesh.hpp
    include/SceneRenderer.hpp
    include/GenericCache.hpp
    include/Image.hpp
    include/ImageProperties.hpp
    include/Instance.hpp
    include/InterfaceSystem.hpp
    include/Logger.hpp
    include/Material.hpp
    include/Math.hpp
    include/Pipeline.hpp
    include/PlatformConfig.hpp
    include/PlatformUI.hpp
    include/RenderingDefinitions.hpp
    include/ResizeDependent.hpp
    include/Shader.hpp
    include/Swapchain.hpp
    include/Texture.hpp
    include/ThreadPool.hpp
    include/Timer.hpp
    include/Types.hpp
    include/UI.hpp
    include/Verify.hpp
    include/Window.hpp
    include/bus/IMessagingAPI.hpp
    include/bus/MessagingClient.hpp
    include/core/Forward.hpp
    include/pch/vkgpgpu_pch.hpp
    inline/Logger.inl
    src/Allocator.cpp
    src/App.cpp
    src/Buffer.cpp
    src/CommandBuffer.cpp
    src/DataBuffer.cpp
    src/DebugMarker.cpp
    src/DescriptorResource.cpp
    src/Device.cpp
    src/DynamicLibraryLoader.cpp
    src/Formatters.cpp
    src/Framebuffer.cpp
    src/GIFTexture.cpp
    src/Image.cpp
    src/Instance.cpp
    src/InterfaceSystem.cpp
    src/Mesh.cpp
    src/SceneRenderer.cpp
    src/Logger.cpp
    src/Material.cpp
    src/Pipeline.cpp
    src/Shader.cpp
    src/Swapchain.cpp
    src/Texture.cpp
    src/Timer.cpp
    src/UI.cpp
    src/Verify.cpp
    src/Window.cpp
)

add_library(Core STATIC ${SOURCES})
target_include_directories(Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/inline
    ${CMAKE_SOURCE_DIR}/Platform
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm
    ${CMAKE_SOURCE_DIR}/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/assimp/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/thread-pool/include)
target_link_libraries(Core PUBLIC ECS Platform Reflection imgui imguizmo assimp::assimp glfw fmt::fmt Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator stbimage::stbimage)
target_precompile_headers(Core PUBLIC include/pch/vkgpgpu_pch.hpp)
target_compile_definitions(Core PUBLIC GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Core PUBLIC GPGPU_DEBUG)
    target_compile_definitions(Core PUBLIC GPGPU_FRAME_COUNT=2)
    set(GPGPU_FAST_LOGGING ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(Core PUBLIC GPGPU_FRAME_COUNT=2)
    target_compile_definitions(Core PUBLIC GPGPU_RELEASE)
    set(GPGPU_FAST_LOGGING ON)
endif()

if(GPGPU_PIPELINE STREQUAL "ON")
    message(STATUS "GPGPU_PIPELINE is ON")
    target_compile_definitions(Core PUBLIC GPGPU_PIPELINE)
else()
    message(STATUS "GPGPU_PIPELINE is OFF")
endif()

if (GPGPU_FAST_LOGGING)
    target_compile_definitions(Core PUBLIC GPGPU_OPTIMIZE_LOGGING)
endif()

# If we're on Linux + clang/gnu, need to set -Wno-nullability-completeness
if(UNIX AND NOT APPLE)
    target_compile_options(Core PRIVATE -Wno-nullability-completeness -Wno-format)
endif()
