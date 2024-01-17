add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(fmt)
add_subdirectory(stb)
add_subdirectory(entt)

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

if(ENABLE_TESTING)
    add_subdirectory(Catch2)
endif()
