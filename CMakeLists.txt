cmake_minimum_required(VERSION 3.21)
project(GLCompute)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(Core)
add_subdirectory(App)
