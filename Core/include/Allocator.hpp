// N.B This class should technically only be included in cpp files.
#pragma once

#include "Logger.hpp"
#include <vk_mem_alloc.h>

namespace Core {

class Allocator {
public:
  static auto get_allocator() -> VmaAllocator {
    static VmaAllocator allocator{construct_allocator()};
    return allocator;
  }

  static void destroy() {
    vmaDestroyAllocator(get_allocator());
    info("Destroyed Allocator!");
  }

private:
  static auto construct_allocator() -> VmaAllocator;
};

} // namespace Core