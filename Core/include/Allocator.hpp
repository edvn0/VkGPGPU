// N.B This class should technically only be included in cpp files.
#pragma once

#include <vk_mem_alloc.h>

namespace Core {

class Allocator {
public:
  static auto get_allocator() -> VmaAllocator {
    static VmaAllocator allocator{construct_allocator()};
    return allocator;
  }

  static void destroy() { vmaDestroyAllocator(get_allocator()); }

private:
  static auto construct_allocator() -> VmaAllocator;
};

} // namespace Core