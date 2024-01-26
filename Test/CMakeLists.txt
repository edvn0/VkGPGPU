cmake_minimum_required(VERSION 3.21)
project(Test)

message(STATUS "VkGPGPU Testing is enabled!")
add_executable(Test
    units/containers/circular_buffer_test.cpp
    units/bus/bus_test.cpp
    units/demo_test.cpp
    units/image/construct_image.cpp
    units/data_buffer/data_buffer_tests.cpp
    units/generic_cache/texture_cache_tests.cpp
)

target_include_directories(Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../Core/include ../Platform/include ${CMAKE_SOURCE_DIR}/ThirdParty/glm)
target_link_libraries(Test PRIVATE Catch2::Catch2WithMain Core Platform Vulkan::Vulkan)
catch_discover_tests(Test)
