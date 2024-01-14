add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(fmt)
add_subdirectory(stb)

if(ENABLE_TESTING)
    add_subdirectory(Catch2)
endif()
