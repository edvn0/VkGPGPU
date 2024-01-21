cmake_minimum_required(VERSION 3.21)
project(App)

add_executable(App ClientApp.hpp main.cpp ClientApp.cpp widgets/Widget.hpp widgets/include/FilesystemWidget.hpp widgets/src/FilesystemWidget.cpp)
target_link_libraries(App PRIVATE Core fmt::fmt Vulkan::Vulkan)
target_include_directories(App PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets/include ${CMAKE_CURRENT_SOURCE_DIR}/widgets)
target_sources(App PRIVATE version.rc)
