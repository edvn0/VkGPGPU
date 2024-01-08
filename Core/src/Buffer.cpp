#include "pch/vkgpgpu_pch.hpp"

#include "Buffer.hpp"

#include <cassert>
#include <cstring>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "Allocator.hpp"
#include "DebugMarker.hpp"
#include "Device.hpp"
#include "Verify.hpp"

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

Buffer::Buffer(const Device &dev, u64 input_size, Type buffer_type)
    : device(dev), buffer_data(make_scope<BufferDataImpl>()), size(input_size),
      type(buffer_type) {
  initialise_vulkan_buffer();
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

  DebugMarker::set_object_name(device.get_device(), buffer_data->buffer,
                               VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
                               fmt::format("Buffer-{}", type).data());
}

void Buffer::initialise_vertex_buffer() {}

void Buffer::initialise_index_buffer() {}

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
  Allocator allocator{"Buffer"};
  allocator.deallocate_buffer(buffer_data->allocation, buffer_data->buffer);
  info("Destroyed Buffer! (type: {})", type);
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
