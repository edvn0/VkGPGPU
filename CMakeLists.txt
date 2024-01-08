cmake_minimum_required(VERSION 3.21)
project(VkGPGPU)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(fmt REQUIRED)

set(CMAKE_C_FLAGS_INIT "-fsanitize=undefined")
set(CMAKE_CXX_FLAGS_INIT "-fsanitize=undefined")

add_subdirectory(Core)
add_subdirectory(App)
add_subdirectory(Platform)
