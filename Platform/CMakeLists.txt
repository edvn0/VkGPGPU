cmake_minimum_required(VERSION 3.21)
project(Platform)


if(WIN32)
    set(SOURCES windows/include/Loader.hpp windows/src/Loader.cpp)
    add_library(Platform STATIC ${SOURCES})
else()
    set(SOURCES linux/include/Loader.hpp linux/src/Loader.cpp)
    add_library(Platform STATIC ${SOURCES})
endif()

target_include_directories(Platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../Core/include)
