#include "CommandBuffer.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Device.hpp"
#include "Verify.hpp"

#include <array>
#include <limits>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

class NoQueueTypeException final : public BaseException {
public:
  using BaseException::BaseException;
};

static constexpr auto timeout = std::numeric_limits<u64>::max();

auto create_immediate(const Device &device, Queue::Type type)
    -> ImmediateCommandBuffer {
  return ImmediateCommandBuffer(device, type);
}

ImmediateCommandBuffer::ImmediateCommandBuffer(const Device &device,
                                               Queue::Type queue_type)
    : type(queue_type) {

  const auto appropriate_commandbuffer_type = [&]() {
    switch (type) {
    case Queue::Type::Compute:
      return CommandBuffer::Type::Compute;
    case Queue::Type::Graphics:
      return CommandBuffer::Type::Graphics;
    case Queue::Type::Transfer:
      return CommandBuffer::Type::Transfer;
    default:
      throw NoQueueTypeException(
          fmt::format("Unknown queue type. Chosen was: {}", type));
    }
  }();
  command_buffer =
      make_scope<CommandBuffer>(device, appropriate_commandbuffer_type);
  command_buffer->begin(0, type);
}

ImmediateCommandBuffer::~ImmediateCommandBuffer() {
  try {
    command_buffer->end_and_submit(type);
  } catch (...) {
    error("Failed to submit command buffer");
  }
}

auto ImmediateCommandBuffer::get_command_buffer() const -> VkCommandBuffer {
  return command_buffer->get_command_buffer();
}

CommandBuffer::CommandBuffer(const Device &dev, CommandBuffer::Type type,
                             u32 input_frame_count)
    : device(dev), frame_count(input_frame_count) {
  switch (type) {
  case Type::Compute:
    queue_type = Queue::Type::Compute;
    break;
  case Type::Graphics:
    queue_type = Queue::Type::Graphics;
    break;
  case Type::Transfer:
    queue_type = Queue::Type::Transfer;
    break;
  default:
    throw NoQueueTypeException("Unknown queue type");
  }

  supports_device_query =
      device.check_support(Feature::DeviceQuery, queue_type);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = device.get_family_index(Queue::Type::Compute);
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  verify(vkCreateCommandPool(device.get_device(), &pool_info, nullptr,
                             &command_pool),
         "vkCreateCommandPool", "Failed to create command pool");

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<u32>(command_buffers.size());

  for (auto &command_buffer : command_buffers) {
    verify(vkAllocateCommandBuffers(device.get_device(), &alloc_info,
                                    &command_buffer.command_buffer),
           "vkAllocateCommandBuffers", "Failed to allocate command buffers");
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto i = 0U; i < frame_count; ++i) {
    verify(vkCreateFence(device.get_device(), &fence_info, nullptr,
                         &command_buffers[i].compute_in_flight_fence),
           "vkCreateFence", "Failed to create fence");
    verify(vkCreateFence(device.get_device(), &fence_info, nullptr,
                         &command_buffers[i].graphics_in_flight_fence),
           "vkCreateFence", "Failed to create fence");
  }

  // Create semaphores
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto i = 0U; i < frame_count; ++i) {
    verify(vkCreateSemaphore(device.get_device(), &semaphore_info, nullptr,
                             &command_buffers[i].compute_finished_semaphore),
           "vkCreateSemaphore", "Failed to create semaphore");

    verify(vkCreateSemaphore(device.get_device(), &semaphore_info, nullptr,
                             &command_buffers[i].image_available_semaphore),
           "vkCreateSemaphore", "Failed to create semaphore");

    verify(vkCreateSemaphore(device.get_device(), &semaphore_info, nullptr,
                             &command_buffers[i].render_finished_semaphore),
           "vkCreateSemaphore", "Failed to create semaphore");
  }

  if (supports_device_query) {
    create_query_objects();
  }
}

CommandBuffer::~CommandBuffer() {
  vkDeviceWaitIdle(device.get_device());
  if (supports_device_query) {
    destroy_query_objects();
  }

  vkDestroyCommandPool(device.get_device(), command_pool, nullptr);

  for (const auto &command_buffer : command_buffers) {
    vkDestroyFence(device.get_device(), command_buffer.compute_in_flight_fence,
                   nullptr);
    vkDestroyFence(device.get_device(), command_buffer.graphics_in_flight_fence,
                   nullptr);
  }

  // Destroy semaphores
  for (const auto &command_buffer : command_buffers) {
    vkDestroySemaphore(device.get_device(),
                       command_buffer.compute_finished_semaphore, nullptr);
    vkDestroySemaphore(device.get_device(),
                       command_buffer.image_available_semaphore, nullptr);
    vkDestroySemaphore(device.get_device(),
                       command_buffer.render_finished_semaphore, nullptr);
  }
}

auto CommandBuffer::begin(u32 provided_frame, Queue::Type type) -> void {
  active_frame = &command_buffers.at(provided_frame);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  const auto chosen_fence = type == Queue::Type::Compute
                                ? &active_frame->compute_in_flight_fence
                                : &active_frame->graphics_in_flight_fence;
  verify(vkResetCommandBuffer(active_frame->command_buffer, 0),
         "vkResetCommandBuffer", "Failed to reset command buffer");
  verify(vkBeginCommandBuffer(active_frame->command_buffer, &begin_info),
         "vkBeginCommandBuffer", "Failed to begin recording command buffer");
  verify(
      vkWaitForFences(device.get_device(), 1, chosen_fence, VK_TRUE, timeout),
      "vkWaitForFences", "Failed to wait for fence");
  if (supports_device_query) {
    vkCmdResetQueryPool(active_frame->command_buffer, active_frame->query_pool,
                        0, 2);
    vkCmdWriteTimestamp(active_frame->command_buffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        active_frame->query_pool, 0);
  }
}

auto CommandBuffer::submit(Queue::Type type) const -> void {
  if (type == Queue::Type::Compute) {
    const auto vk_device = device.get_device();
    vkWaitForFences(vk_device, 1, &active_frame->compute_in_flight_fence,
                    VK_TRUE, UINT64_MAX);
    vkResetFences(vk_device, 1, &active_frame->compute_in_flight_fence);

    VkSubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &active_frame->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &active_frame->compute_finished_semaphore;

    verify(vkQueueSubmit(device.get_queue(Queue::Type::Compute), 1,
                         &submit_info, active_frame->compute_in_flight_fence),
           "vkQueueSubmit", "Failed to submit queue");
  } else {
    const auto vk_device = device.get_device();
    vkWaitForFences(vk_device, 1, &active_frame->graphics_in_flight_fence,
                    VK_TRUE, UINT64_MAX);
    vkResetFences(vk_device, 1, &active_frame->graphics_in_flight_fence);

    VkSubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &active_frame->command_buffer;

    const std::array wait_semaphores = {
        active_frame->compute_finished_semaphore,
        active_frame->image_available_semaphore,
    };
    constexpr VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pSignalSemaphores = &active_frame->render_finished_semaphore;
    submit_info.signalSemaphoreCount = 1;

    verify(vkQueueSubmit(device.get_queue(Queue::Type::Graphics), 1,
                         &submit_info, active_frame->graphics_in_flight_fence),
           "vkQueueSubmit", "Failed to submit queue");
  }
}

auto CommandBuffer::end() const -> void {
  if (supports_device_query) {
    vkCmdWriteTimestamp(active_frame->command_buffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        active_frame->query_pool, 1);
  }

  verify(vkEndCommandBuffer(active_frame->command_buffer), "vkEndCommandBuffer",
         "Failed to record command buffer");
}

auto CommandBuffer::end_and_submit(Queue::Type type) const -> void {
  end();
  submit(type);
}

void CommandBuffer::create_query_objects() {
  VkQueryPoolCreateInfo query_pool_info{};
  query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  query_pool_info.queryCount = 2;

  for (auto i = 0U; i < frame_count; ++i) {
    verify(vkCreateQueryPool(device.get_device(), &query_pool_info, nullptr,
                             &command_buffers.at(i).query_pool),
           "vkCreateQueryPool", "Failed to create query pool");
  }
}

void CommandBuffer::destroy_query_objects() const {
  for (auto i = 0U; i < frame_count; ++i) {
    vkDestroyQueryPool(device.get_device(), command_buffers.at(i).query_pool,
                       nullptr);
  }
}

} // namespace Core
