#include "pch/vkgpgpu_pch.hpp"

#include "Buffer.hpp"

#include <cassert>
#include <cstring>
#include <vk_mem_alloc.h>

#include "Allocator.hpp"
#include "DebugMarker.hpp"
#include "Device.hpp"
#include "Verify.hpp"

namespace Core {

struct BufferDataImpl {
  VkBuffer buffer{};
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
};

Buffer::Buffer(u64 input_size, Type buffer_type)
    : size(input_size), type(buffer_type),
      buffer_data(make_scope<BufferDataImpl>()) {
  initialise_vulkan_buffer();
}

void Buffer::initialise_vulkan_buffer() {
  switch (type) {
  case Type::Vertex:
    initialise_vertex_buffer();
    break;
  case Type::Index:
    initialise_index_buffer();
    break;
  case Type::Uniform:
    initialise_uniform_buffer();
    break;
  case Type::Storage:
    initialise_storage_buffer();
    break;
  }

  DebugMarker::set_object_name(Device::get()->get_device(), buffer_data->buffer,
                               VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
                               fmt::format("Buffer-{}", type).data());
}

void Buffer::initialise_vertex_buffer() {}
void Buffer::initialise_index_buffer() {}

void Buffer::initialise_uniform_buffer() {
  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = size;
  buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocation_create_info{};
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  allocation_create_info.flags =
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  verify(vmaCreateBuffer(Allocator::get_allocator(), &buffer_create_info,
                         &allocation_create_info, &buffer_data->buffer,
                         &buffer_data->allocation,
                         &buffer_data->allocation_info),
         "vmaCreateBuffer", "Failed to create buffer");

  descriptor_info.buffer = buffer_data->buffer;
  descriptor_info.offset = 0;
  descriptor_info.range = size;
}

void Buffer::initialise_storage_buffer() {
  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = size;
  buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocation_create_info{};
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  allocation_create_info.flags =
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  verify(vmaCreateBuffer(Allocator::get_allocator(), &buffer_create_info,
                         &allocation_create_info, &buffer_data->buffer,
                         &buffer_data->allocation,
                         &buffer_data->allocation_info),
         "vmaCreateBuffer", "Failed to create buffer");

  descriptor_info.buffer = buffer_data->buffer;
  descriptor_info.offset = 0;
  descriptor_info.range = size;
}

auto Buffer::read_raw(size_t offset, size_t data_size) -> std::vector<char> {
  assert(offset + data_size <= size); // Ensure we don't read out of bounds

  std::vector<char> data(data_size);
  const auto is_always_mapped =
      buffer_data->allocation_info.pMappedData != nullptr;
  if (is_always_mapped) {
    // If the buffer is always mapped, copy the data directly
    std::memcpy(
        data.data(),
        static_cast<const char *>(buffer_data->allocation_info.pMappedData) +
            offset,
        data_size);
  } else {
    // Otherwise, map the buffer, copy the data, then unmap
    void *mapped_data{};
    verify(vmaMapMemory(Allocator::get_allocator(), buffer_data->allocation,
                        &mapped_data),
           "vmaMapMemory", "Failed to map memory");
    std::memcpy(data.data(), static_cast<const char *>(mapped_data) + offset,
                data_size);
    vmaUnmapMemory(Allocator::get_allocator(), buffer_data->allocation);
  }
  return data;
}

Buffer::~Buffer() {
  vmaDestroyBuffer(Allocator::get_allocator(), buffer_data->buffer,
                     buffer_data->allocation);
  buffer_data.reset();
}

auto Buffer::get_buffer() const noexcept -> VkBuffer {
  return buffer_data->buffer;
}

void Buffer::write(const void *data, u64 data_size) {
  const auto is_always_mapped = buffer_data->allocation_info.pMappedData;
  if (is_always_mapped) {
    std::memcpy(buffer_data->allocation_info.pMappedData, data, data_size);
  } else {
    void *mapped_data{};
    verify(vmaMapMemory(Allocator::get_allocator(), buffer_data->allocation,
                        &mapped_data),
           "vmaMapMemory", "Failed to map memory");
    std::memcpy(mapped_data, data, data_size);
    vmaUnmapMemory(Allocator::get_allocator(), buffer_data->allocation);
  }
}

} // namespace Core
