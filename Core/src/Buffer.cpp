#include "pch/vkgpgpu_pch.hpp"

#include "Buffer.hpp"

#include "Allocator.hpp"
#include "DataBuffer.hpp" // for human_readable_size
#include "DebugMarker.hpp"
#include "Device.hpp"
#include "Verify.hpp"

#include <cassert>
#include <cstring>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace Core {

static auto to_vulkan_usage(Buffer::Type buffer_type) -> VkBufferUsageFlags {
  switch (buffer_type) {
  case Buffer::Type::Vertex:
    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  case Buffer::Type::Index:
    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  case Buffer::Type::Uniform:
    return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  case Buffer::Type::Storage:
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  default:
    return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    assert(false);
  }
}

struct BufferDataImpl {
  VkBuffer buffer{};
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
};

Buffer::Buffer(const Device &dev, u64 input_size, Type buffer_type,
               u32 input_binding)
    : device(&dev), buffer_data(make_scope<BufferDataImpl>()), size(input_size),
      type(buffer_type), binding(input_binding) {
  initialise_vulkan_buffer();
  initialise_descriptor_info();
}

auto Buffer::construct(const Device &device, u64 input_size, Type buffer_type,
                       u32 binding) -> Scope<Buffer> {
  return make_scope<Buffer>(device, input_size, buffer_type, binding);
}

auto Buffer::construct(const Device &device, u64 input_size, Type buffer_type)
    -> Scope<Buffer> {
  return make_scope<Buffer>(device, input_size, buffer_type, invalid_binding);
}

auto Buffer::initialise_descriptor_info() -> void {
  descriptor_info.buffer = buffer_data->buffer;
  descriptor_info.offset = 0;
  descriptor_info.range = size;
}

void Buffer::initialise_vulkan_buffer() {
  switch (type) {
    using enum Core::Buffer::Type;
  case Vertex:
    initialise_vertex_buffer();
    break;
  case Index:
    initialise_index_buffer();
    break;
  case Uniform:
    initialise_uniform_buffer();
    break;
  case Storage:
    initialise_storage_buffer();
    break;
  case Invalid:
    assert(false);
    break;
  }

  DebugMarker::set_object_name(*device, buffer_data->buffer,
                               VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
                               fmt::format("Buffer-{}", type).data());
}

void Buffer::initialise_vertex_buffer() {
  Allocator allocator{"Vertex Buffer"};

  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = get_size();
  buffer_create_info.usage = to_vulkan_usage(type);

  const auto is_vertex = type == Buffer::Type::Vertex;

  auto usage = Usage::AUTO_PREFER_DEVICE;

  buffer_data->allocation = allocator.allocate_buffer(
      buffer_data->buffer, buffer_data->allocation_info, buffer_create_info,
      {
          .usage = usage,
      });
}

void Buffer::initialise_index_buffer() {
  Allocator allocator{"Index Buffer"};

  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = get_size();
  buffer_create_info.usage = to_vulkan_usage(type);

  auto usage = Usage::AUTO_PREFER_DEVICE;

  buffer_data->allocation = allocator.allocate_buffer(
      buffer_data->buffer, buffer_data->allocation_info, buffer_create_info,
      {
          .usage = usage,
      });
}

void Buffer::initialise_uniform_buffer() {
  Allocator allocator{"Uniform Buffer"};

  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = get_size();
  buffer_create_info.usage = to_vulkan_usage(type);

  const auto is_uniform = type == Buffer::Type::Uniform;

  auto usage = is_uniform ? Usage::AUTO_PREFER_HOST : Usage::AUTO_PREFER_DEVICE;
  auto creation = Creation::MAPPED_BIT;
  if (is_uniform) {
    creation |= Creation::HOST_ACCESS_RANDOM_BIT;
  }

  buffer_data->allocation = allocator.allocate_buffer(
      buffer_data->buffer, buffer_data->allocation_info, buffer_create_info,
      {
          .usage = usage,
          .creation = creation,
      });
}

void Buffer::initialise_storage_buffer() {
  Allocator allocator{"Storage Buffer"};

  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = get_size();
  buffer_create_info.usage = to_vulkan_usage(type);

  const auto is_uniform = type == Buffer::Type::Uniform;

  auto usage = is_uniform ? Usage::AUTO_PREFER_HOST : Usage::AUTO_PREFER_DEVICE;
  auto creation = Creation::MAPPED_BIT;
  if (is_uniform) {
    creation |= Creation::HOST_ACCESS_RANDOM_BIT;
  }

  buffer_data->allocation = allocator.allocate_buffer(
      buffer_data->buffer, buffer_data->allocation_info, buffer_create_info,
      {
          .usage = usage,
          .creation = creation,
      });
}

auto Buffer::read_raw(size_t offset, size_t data_size) -> std::vector<char> {
  assert(offset + data_size <= size); // Ensure we don't read out of bounds

  std::vector<char> data(data_size);
  const auto is_always_mapped =
      buffer_data->allocation_info.pMappedData != nullptr;
  if (is_always_mapped) {
    std::memcpy(
        data.data(),
        static_cast<const char *>(buffer_data->allocation_info.pMappedData) +
            offset,
        data_size);
  } else {
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
  Allocator allocator{"Buffer"};
  allocator.deallocate_buffer(buffer_data->allocation, buffer_data->buffer);
  debug("Destroyed buffer (type: {}, size: {})", type,
        human_readable_size(size));
}

auto Buffer::get_buffer() const noexcept -> VkBuffer {
  return buffer_data->buffer;
}

void Buffer::write(const void *data, u64 data_size, u64 offset) {
  assert(offset + data_size <= size); // Ensure we don't write out of bounds

  const auto is_always_mapped = buffer_data->allocation_info.pMappedData;
  if (is_always_mapped) {
    std::memcpy(static_cast<char *>(buffer_data->allocation_info.pMappedData) +
                    offset,
                data, data_size);
  } else {
    void *mapped_data{};
    verify(vmaMapMemory(Allocator::get_allocator(), buffer_data->allocation,
                        &mapped_data),
           "vmaMapMemory", "Failed to map memory");
    std::memcpy(static_cast<u8 *>(mapped_data) + offset, data, data_size);
    vmaUnmapMemory(Allocator::get_allocator(), buffer_data->allocation);
  }
}

void Buffer::write(const void *data, u64 data_size) {
  write(data, data_size, 0);
}

void Buffer::resize(u64 new_size) {
  if (size == new_size)
    return;

  // Step 1: Destroy the current buffer and its allocation
  Allocator allocator{"Buffer"};
  allocator.deallocate_buffer(buffer_data->allocation, buffer_data->buffer);

  // Step 2: Update the size
  size = new_size;

  // Step 3: Recreate the buffer with the new size
  initialise_vulkan_buffer();
  initialise_descriptor_info();

  // Optional: Debug message to indicate successful resize
  debug("Resized buffer (type: {}, new size: {})", type,
        human_readable_size(new_size));
}

auto Buffer::get_vulkan_type() const noexcept -> VkDescriptorType {
  switch (type) {
  case Type::Uniform:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case Type::Storage:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  default:
    return unreachable_return<VK_DESCRIPTOR_TYPE_MAX_ENUM>();
  }
}

void Buffer::write(const void *data, u64 data_size, u64 offset) const {
  assert(data_size <= size); // Ensure we don't write out of bounds

  const auto is_always_mapped = buffer_data->allocation_info.pMappedData;
  if (is_always_mapped) {
    std::memcpy(static_cast<u8 *>(buffer_data->allocation_info.pMappedData) +
                    offset,
                data, data_size);
  } else {
    void *mapped_data{};
    verify(vmaMapMemory(Allocator::get_allocator(), buffer_data->allocation,
                        &mapped_data),
           "vmaMapMemory", "Failed to map memory");
    std::memcpy(static_cast<u8 *>(mapped_data) + offset, data, data_size);
    vmaUnmapMemory(Allocator::get_allocator(), buffer_data->allocation);
  }
}

} // namespace Core
