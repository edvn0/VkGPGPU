#include "CommandBuffer.hpp"

#include "Device.hpp"
#include "Verify.hpp"

#include <array>
#include <limits>
#include <vulkan/vulkan.h>

namespace Core {

static constexpr auto timeout = std::numeric_limits<u64>::max();

CommandBuffer::CommandBuffer(Type type)
    : supports_device_query(Device::get()->check_support(
          Feature::DeviceQuery, Queue::Type::Compute)) {
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

  const auto device = Device::get()->get_device();

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex =
      Device::get()->get_family_index(Queue::Type::Compute);
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  verify(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool),
         "vkCreateCommandPool", "Failed to create command pool");

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

  for (auto i = 0U; i < Config::frame_count; ++i) {
    verify(
        vkCreateFence(device, &fence_info, nullptr, &command_buffers[i].fence),
        "vkCreateFence", "Failed to create fence");
  }

  // Create semaphores
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto i = 0U; i < Config::frame_count; ++i) {
    verify(vkCreateSemaphore(device, &semaphore_info, nullptr,
                             &command_buffers[i].finished_semaphore),
           "vkCreateSemaphore", "Failed to create semaphore");
  }

  if (supports_device_query) {
    create_query_objects();
  } else {
    query_pools.fill(VK_NULL_HANDLE);
  }
}

CommandBuffer::~CommandBuffer() {
  if (supports_device_query) {
    destroy_query_objects();
  }

  const auto &device = Device::get()->get_device();
  vkDeviceWaitIdle(device);

  vkDestroyCommandPool(device, command_pool, nullptr);

  for (const auto &command_buffer : command_buffers) {
    vkDestroyFence(device, command_buffer.fence, nullptr);
  }

  // Destroy semaphores
  for (const auto &command_buffer : command_buffers) {
    vkDestroySemaphore(device, command_buffer.finished_semaphore, nullptr);
  }
}

auto CommandBuffer::begin(u32 provided_frame) -> void {
  active_frame = &command_buffers.at(provided_frame);
  active_pool = &query_pools.at(provided_frame);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  verify(vkBeginCommandBuffer(active_frame->command_buffer, &begin_info),
         "vkBeginCommandBuffer", "Failed to begin recording command buffer");
  verify(vkWaitForFences(Device::get()->get_device(), 1, &active_frame->fence, VK_TRUE, timeout),
       "vkWaitForFences", "Failed to wait for fence");
  if (supports_device_query) {
    vkCmdResetQueryPool(active_frame->command_buffer, *active_pool, 0, 2);
    vkCmdWriteTimestamp(active_frame->command_buffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, *active_pool, 0);
  }
}

auto CommandBuffer::submit() -> void {
  const auto device = Device::get()->get_device();

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  constexpr VkPipelineStageFlags wait_stage_mask =
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  submit_info.pWaitDstStageMask = &wait_stage_mask;
 // submit_info.signalSemaphoreCount = 1;
 // submit_info.pSignalSemaphores = &active_frame->finished_semaphore;

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &active_frame->command_buffer;

  verify(vkWaitForFences(device, 1, &active_frame->fence, VK_TRUE, timeout),
       "vkWaitForFences", "Failed to wait for fence");
  verify(vkResetFences(device, 1, &active_frame->fence), "vkResetFences",
         "Failed to reset fence");

  verify(vkQueueSubmit(Device::get()->get_queue(Queue::Type::Compute), 1,
                       &submit_info, active_frame->fence),
         "vkQueueSubmit", "Failed to submit queue");
  verify(vkWaitForFences(device, 1, &active_frame->fence, VK_TRUE, timeout),
         "vkWaitForFences", "Failed to wait for fence");

  if (supports_device_query) {
    std::array<u64, 2> timestamps{};
    vkGetQueryPoolResults(device, *active_pool, 0, 2, sizeof(timestamps),
                          timestamps.data(), sizeof(u64),
                          VK_QUERY_RESULT_64_BIT);

    const auto timestamp_period =
        Device::get()->get_device_properties().limits.timestampPeriod;
    static constexpr auto convert_to_double = [](const auto timestamp) -> double {
      return static_cast<double>(timestamp);
    };
    double timeTakenInSeconds =
        (convert_to_double(timestamps[1]) - convert_to_double(timestamps[0])) *
        timestamp_period * 1e-9;

    info("Time taken: {} seconds", timeTakenInSeconds);
  }
}

auto CommandBuffer::end() -> void {
  if (supports_device_query) {
    vkCmdWriteTimestamp(active_frame->command_buffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, *active_pool, 1);
  }

  verify(vkEndCommandBuffer(active_frame->command_buffer), "vkEndCommandBuffer",
         "Failed to record command buffer");
}

auto CommandBuffer::end_and_submit() -> void {
  end();
  submit();
}

void CommandBuffer::create_query_objects() {
  const auto device = Device::get()->get_device();

  VkQueryPoolCreateInfo query_pool_info{};
  query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  query_pool_info.queryCount = 2;

  for (auto i = 0U; i < Config::frame_count; ++i) {
    verify(vkCreateQueryPool(device, &query_pool_info, nullptr,
                             &query_pools[i]),
           "vkCreateQueryPool", "Failed to create query pool");
  }
}

void CommandBuffer::destroy_query_objects() {
  const auto device = Device::get()->get_device();

  for (auto i = 0U; i < Config::frame_count; ++i) {
    vkDestroyQueryPool(device, query_pools[i], nullptr);
  }
}

} // namespace Core