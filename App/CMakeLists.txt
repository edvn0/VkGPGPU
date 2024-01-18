cmake_minimum_required(VERSION 3.21)
project(App)

add_executable(App ClientApp.hpp main.cpp ClientApp.cpp)
target_link_libraries(App PRIVATE Core fmt::fmt Vulkan::Vulkan)
target_include_directories(App PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include "C:\\Program Files\\RenderDoc")
target_sources(App PRIVATE version.rc)
