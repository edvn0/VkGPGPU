// N.B This class should technically only be included in cpp files.
#pragma once

#include <vk_mem_alloc.h>

namespace Core {

class Allocator {
  struct AllocatorImpl {
    VmaAllocator allocator{};
    AllocatorImpl() = default;
    ~AllocatorImpl() { vmaDestroyAllocator(allocator); }
  };

public:
  static auto get_allocator() -> VmaAllocator {
    static VmaAllocator allocator{construct_allocator()};
    return allocator;
  }

private:
  static auto construct_allocator() -> VmaAllocator;
};

} // namespace Core