cmake_minimum_required(VERSION 3.21)
project(ECS)

set(SOURCES
    include/ECS/Entity.hpp
    include/ECS/Scene.hpp
    src/ECS/Entity.cpp
    src/ECS/Scene.cpp
)
add_library(ECS STATIC ${SOURCES})
target_link_libraries(ECS PUBLIC  EnTT::EnTT)
target_include_directories(ECS PUBLIC
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Core/inline
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm)
target_precompile_headers(App REUSE_FROM Core)
