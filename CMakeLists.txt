cmake_minimum_required(VERSION 3.21)
project(VkGPGPU)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(ThirdParty)
add_subdirectory(Core)
add_subdirectory(App)
add_subdirectory(Platform)
