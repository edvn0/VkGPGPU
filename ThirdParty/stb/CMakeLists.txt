cmake_minimum_required(VERSION 3.21)
project(stbimage)

add_library(stbimage STATIC src/stb_image.cpp)
target_include_directories(stbimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(stbimage::stbimage ALIAS stbimage)
