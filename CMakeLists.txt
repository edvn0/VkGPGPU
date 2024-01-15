cmake_minimum_required(VERSION 3.21)
project(VkGPGPU)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(ThirdParty)
add_subdirectory(Core)
add_subdirectory(App)
add_subdirectory(Platform)

# IF enable testing
if(ENABLE_TESTING)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/ThirdParty/Catch2/extras)
    include(Catch)
    include(CTest)
    enable_testing()
    add_subdirectory(Test)
endif()

add_subdirectory(Reflection)
