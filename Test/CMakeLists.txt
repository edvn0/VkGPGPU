cmake_minimum_required(VERSION 3.21)
project(Test)

message(STATUS "VkGPGPU Testing is enabled!")
add_executable(Test units/demo_test.cpp units/image/construct_image.cpp units/data_buffer/data_buffer_tests.cpp)

target_include_directories(Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../Core/include ../Platform/include)
target_link_libraries(Test PRIVATE Catch2::Catch2WithMain Core Platform Vulkan::Vulkan)
catch_discover_tests(Test)
