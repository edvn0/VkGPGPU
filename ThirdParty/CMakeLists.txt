add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(fmt)
add_subdirectory(stb)

function(add_spirv_cross)
    set(SPIRV_CROSS_CLI OFF)
    set(SPIRV_CROSS_ENABLE_TESTS OFF)

    set(SPIRV_CROSS_ENABLE_GLSL ON)
    set(SPIRV_CROSS_ENABLE_HLSL OFF)
    set(SPIRV_CROSS_ENABLE_MSL OFF)
    set(SPIRV_CROSS_ENABLE_CPP OFF)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF)
    set(SPIRV_CROSS_ENABLE_C_API OFF)
    set(SPIRV_CROSS_ENABLE_UTIL OFF)

    add_subdirectory(SPIRV-Cross)
endfunction()

add_spirv_cross()

if(ENABLE_TESTING)
    add_subdirectory(Catch2)
endif()