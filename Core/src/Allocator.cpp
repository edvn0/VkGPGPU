#include "Logger.hpp"
#include "pch/vkgpgpu_pch.hpp"
#include <cassert>

#include "Allocator.hpp"

#define VMA_DEBUG_LOG_FORMAT(format, ...)                                      \
  do {                                                                         \
    printf((format), __VA_ARGS__);                                             \
    printf("\n");                                                              \
  } while (false)
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Device.hpp"
#include "Instance.hpp"
#include "Verify.hpp"

namespace Core {

auto ensure(bool condition, const std::string &message) -> void {
  assert(condition);
}

Allocator::Allocator(const std::string &resource) : resource_name(resource) {}

auto Allocator::allocate_buffer(VkBuffer &buffer,
                                VkBufferCreateInfo &buffer_info,
                                const AllocationProperties &props)
    -> VmaAllocation {
  ensure(allocator != nullptr, "Allocator was null.");
  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = static_cast<VmaMemoryUsage>(props.usage);
  alloc_info.flags = static_cast<VmaAllocationCreateFlags>(props.creation);

  VmaAllocation allocation{};
  verify(vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &buffer,
                         &allocation, nullptr),
         "vmaCreateBuffer", "Failed to create buffer");
  vmaSetAllocationName(allocator, allocation, resource_name.data());

  return allocation;
}

auto Allocator::allocate_buffer(VkBuffer &buffer,
                                VmaAllocationInfo &allocation_info,
                                VkBufferCreateInfo &buffer_info,
                                const AllocationProperties &props)
    -> VmaAllocation {
  ensure(allocator != nullptr, "Allocator was null.");
  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = static_cast<VmaMemoryUsage>(props.usage);
  alloc_info.flags = static_cast<VmaAllocationCreateFlags>(props.creation) |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

  VmaAllocation allocation{};
  verify(vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &buffer,
                         &allocation, &allocation_info),
         "vmaCreateBuffer", "Failed to create buffer");
  vmaSetAllocationName(allocator, allocation, resource_name.data());

  return allocation;
}

auto Allocator::allocate_image(VkImage &image,
                               VkImageCreateInfo &image_create_info,
                               const AllocationProperties &props)
    -> VmaAllocation {
  ensure(allocator != nullptr, "Allocator was null.");
  VmaAllocationCreateInfo allocation_create_info = {};
  allocation_create_info.usage = static_cast<VmaMemoryUsage>(props.usage);

  VmaAllocation allocation{};
  verify(vmaCreateImage(allocator, &image_create_info, &allocation_create_info,
                        &image, &allocation, nullptr),
         "vmaCreateImage", "Failed to create image");
  vmaSetAllocationName(allocator, allocation, resource_name.data());

  return allocation;
}

void Allocator::deallocate_buffer(VmaAllocation allocation, VkBuffer &buffer) {
  ensure(allocator != nullptr, "Allocator was null.");
  vmaDestroyBuffer(allocator, buffer, allocation);
}

void Allocator::deallocate_image(VmaAllocation allocation, VkImage &image) {
  ensure(allocator != nullptr, "Allocator was null.");
  vmaDestroyImage(allocator, image, allocation);
}

void Allocator::unmap_memory(VmaAllocation allocation) {
  vmaUnmapMemory(allocator, allocation);
}

auto Allocator::construct_allocator(const Device &device,
                                    const Instance &instance) -> VmaAllocator {
  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.physicalDevice = device.get_physical_device();
  allocator_create_info.device = device.get_device();
  allocator_create_info.instance = instance.get_instance();

  VmaAllocator alloc{};
  verify(vmaCreateAllocator(&allocator_create_info, &alloc),
         "vmaCreateAllocator", "Failed to create allocator");
  return alloc;
}

} // namespace Core