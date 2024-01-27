cmake_minimum_required(VERSION 3.21)
project(ECS)

set(SOURCES
    include/ecs/components/Component.hpp
    include/ecs/UUID.hpp
    include/ecs/Entity.hpp
    include/ecs/Scene.hpp
    src/ecs/Entity.cpp
    src/ecs/Scene.cpp
)
add_library(ECS STATIC ${SOURCES})
target_link_libraries(ECS PUBLIC fmt::fmt EnTT::EnTT)
target_include_directories(ECS PUBLIC
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Core/inline
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm)
target_precompile_headers(App REUSE_FROM Core)
