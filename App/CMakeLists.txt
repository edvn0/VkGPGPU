cmake_minimum_required(VERSION 3.21)
project(App)

add_executable(App main.cpp)
target_link_libraries(App PRIVATE Core)
target_include_directories(App PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include "C:\\Program Files\\RenderDoc")
