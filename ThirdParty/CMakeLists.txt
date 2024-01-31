add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(fmt)
add_subdirectory(stb)
add_subdirectory(entt)
add_subdirectory(glfw)

function(add_spirv_cross)
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "Enable SPIRV-Cross static library")
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "Disable SPIRV-Cross CLI")
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Disable SPIRV-Cross tests")

    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "Enable SPIRV-Cross GLSL")
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "Disable SPIRV-Cross HLSL")
    set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "Disable SPIRV-Cross MSL")
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Disable SPIRV-Cross CPP")
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "Disable SPIRV-Cross Reflect")
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Disable SPIRV-Cross C API")
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Disable SPIRV-Cross Util")

    add_subdirectory(SPIRV-Cross)
endfunction()

add_spirv_cross()

function(add_imgui)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )

    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
endfunction()

# Function to add a header-only library
function(add_header_only_library LIB_NAME INCLUDE_PATH)
    add_library(${LIB_NAME} INTERFACE)
    target_include_directories(${LIB_NAME} INTERFACE ${INCLUDE_PATH})
endfunction()

# Assuming your 'reponame' folder is in the current source directory
set(REPONAME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thread-pool/include)
add_header_only_library(thread-pool ${REPONAME_INCLUDE_DIR})

add_imgui()

function(add_glm)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "")
    set(GLM_ENABLE_SIMD_SSE2 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_SSE3 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_SSSE3 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_SSE4_1 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_SSE4_2 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_AVX ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_AVX2 ON CACHE BOOL "")
    add_subdirectory(glm)
endfunction()

add_glm()

function(add_assimp)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
    set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "")

    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
    set(ASSIMP_IGNORE_GIT_HASH ON CACHE BOOL "")

    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_SMD_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_SIB_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")

    add_subdirectory(assimp)
endfunction()

add_assimp()

if(ENABLE_TESTING)
    add_subdirectory(Catch2)
endif()
