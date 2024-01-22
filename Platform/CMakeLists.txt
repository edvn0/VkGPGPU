cmake_minimum_required(VERSION 3.21)
project(Platform)

set(SOURCES rabbitmq/RabbitMQMessagingAPI.cpp rabbitmq/RabbitMQMessagingAPI.hpp)

if(WIN32)
    list(APPEND SOURCES windows/include/Loader.hpp windows/src/Loader.cpp windows/src/PlatformUI.cpp)
else()
    list(APPEND SOURCES linux/include/Loader.hpp linux/src/Loader.cpp linux/src/PlatformUI.cpp)
endif()

add_library(Platform STATIC ${SOURCES})
target_link_libraries(Platform PUBLIC fmt::fmt PRIVATE Core)

target_include_directories(Platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../Core/include)
