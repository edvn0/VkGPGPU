cmake_minimum_required(VERSION 3.21)
project(VkGPGPU)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

find_program(CCACHE_FOUND ccache)

if(CCACHE_FOUND)
    message("CCACHE is found")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) # Less useful to do it for linking, see edit2
else(CCACHE_FOUND)
    message("CCACHE is NOT found")
endif(CCACHE_FOUND)

add_subdirectory(ThirdParty)
add_subdirectory(Core)
add_subdirectory(App)
add_subdirectory(Platform)
add_subdirectory(ECS)

# IF enable testing
if(ENABLE_TESTING)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/ThirdParty/Catch2/extras)
    include(Catch)
    include(CTest)
    enable_testing()
    add_subdirectory(Test)
endif()

add_subdirectory(Reflection)
