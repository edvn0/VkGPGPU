#include "Allocator.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Device.hpp"
#include "Instance.hpp"
#include "Verify.hpp"

namespace Core {

auto Allocator::construct_allocator() -> VmaAllocator {
  VmaAllocatorCreateInfo allocator_create_info{};
  const auto &device = Device::get();
  const auto& instance = Instance::get();
  allocator_create_info.physicalDevice = device->get_physical_device();
  allocator_create_info.device = device->get_device();
  allocator_create_info.instance = instance->get_instance();

  VmaAllocator allocator{};
  verify(vmaCreateAllocator(&allocator_create_info, &allocator),
         "vmaCreateAllocator", "Failed to create allocator");
  return allocator;
}

} // namespace Core