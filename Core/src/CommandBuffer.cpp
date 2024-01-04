#include "CommandBuffer.hpp"

#include "Device.hpp"
#include "Verify.hpp"

#include <array>
#include <vulkan/vulkan_core.h>

namespace Core {

#ifdef GPGPU_DEBUG
auto check_frame_is_not_negative_one(auto frame) {
  if (frame == -1) {
    throw std::runtime_error("Command buffer not begun");
  }
}
#define DEBUG_CHECK_FRAME(x) check_frame_is_not_negative_one(x)
#else
#define DEBUG_CHECK_FRAME(x)
#endif

CommandBuffer::CommandBuffer(u32 frame_count, Type type) {
  switch (type) {
  case Type::Compute:
    queue_type = Queue::Type::Compute;
    break;
  case Type::Graphics:
    queue_type = Queue::Type::Graphics;
    break;
  default:
    throw std::runtime_error("Unknown queue type");
  }

  auto device = Device::get()->get_device();

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex =
      Device::get()->get_family_index(Queue::Type::Compute);
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  verify(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool),
         "vkCreateCommandPool", "Failed to create command pool");

  command_buffers.resize(frame_count);

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<u32>(command_buffers.size());

  for (auto &command_buffer : command_buffers) {
    verify(vkAllocateCommandBuffers(device, &alloc_info,
                                    &command_buffer.command_buffer),
           "vkAllocateCommandBuffers", "Failed to allocate command buffers");
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto i = 0U; i < frame_count; ++i) {
    verify(
        vkCreateFence(device, &fence_info, nullptr, &command_buffers[i].fence),
        "vkCreateFence", "Failed to create fence");
  }
}

CommandBuffer::~CommandBuffer() {
  auto device = Device::get()->get_device();

  for (auto &command_buffer : command_buffers) {
    vkDestroyFence(device, command_buffer.fence, nullptr);
  }

  vkDestroyCommandPool(device, command_pool, nullptr);
}

auto CommandBuffer::begin(u32 provided_frame) -> void {
  current_frame = static_cast<i32>(provided_frame);
  auto device = Device::get()->get_device();
  // Wait for fence
  DEBUG_CHECK_FRAME(current_frame);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  verify(vkBeginCommandBuffer(command_buffers[current_frame].command_buffer,
                              &begin_info),
         "vkBeginCommandBuffer", "Failed to begin recording command buffer");
}

auto CommandBuffer::submit() -> void {
  DEBUG_CHECK_FRAME(current_frame);
  auto device = Device::get()->get_device();

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers[current_frame].command_buffer;

  verify(vkWaitForFences(device, 1, &command_buffers[current_frame].fence,
                         VK_TRUE, 2'000'000'000),
         "vkWaitForFences", "Failed to wait for fence");
  verify(vkResetFences(device, 1, &command_buffers[current_frame].fence),
         "vkResetFences", "Failed to reset fence");

  verify(vkQueueSubmit(Device::get()->get_queue(Queue::Type::Compute), 1,
                       &submit_info, nullptr),
         "vkQueueSubmit", "Failed to submit queue");

  verify(vkWaitForFences(device, 1, &command_buffers[current_frame].fence,
                         VK_TRUE, 2'000'000'000),
         "vkWaitForFences", "Failed to wait for fence");

  current_frame = -1;
}

auto CommandBuffer::end() -> void {
  auto device = Device::get()->get_device();

  verify(vkEndCommandBuffer(command_buffers[current_frame].command_buffer),
         "vkEndCommandBuffer", "Failed to record command buffer");
}

auto CommandBuffer::submit_and_end() -> void {
  DEBUG_CHECK_FRAME(current_frame);

  end();
  submit();
}

} // namespace Core