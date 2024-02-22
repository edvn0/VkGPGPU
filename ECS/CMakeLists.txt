cmake_minimum_required(VERSION 3.21)
project(ECS)

set(SOURCES
    include/ecs/components/Component.hpp
    src/ecs/components/Component.cpp
    include/ecs/UUID.hpp
    include/ecs/Entity.hpp
    include/ecs/Scene.hpp
    src/ecs/Entity.cpp
    src/ecs/Scene.cpp
    include/ecs/serialisation/Serialisers.hpp
    src/ecs/serialisation/Serialisers.cpp
    include/ecs/serialisation/SceneSerialiser.hpp
    src/ecs/serialisation/SceneSerialiser.cpp
)
add_library(ECS STATIC ${SOURCES})
target_link_libraries(ECS PUBLIC Reflection fmt::fmt EnTT::EnTT Vulkan::Vulkan magic_enum)
target_compile_definitions(ECS PUBLIC GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)
target_include_directories(ECS PUBLIC
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Core/inline
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm
    ${CMAKE_SOURCE_DIR}/ThirdParty/thread-pool/include)
target_precompile_headers(App REUSE_FROM Core)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(ECS PRIVATE GPGPU_DEBUG)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(ECS PRIVATE GPGPU_RELEASE)
endif()
