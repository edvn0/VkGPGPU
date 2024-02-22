
cmake_minimum_required(VERSION 3.21)
project(imgui-notify)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(${PROJECT_NAME} INTERFACE -DFONT_ICON_FILE_NAME_FAR="${CMAKE_SOURCE_DIR}/App/editor/fa-regular-400.ttf" -DFONT_ICON_FILE_NAME_FAS="${CMAKE_SOURCE_DIR}/App/editor/fa-solid-900.ttf")
