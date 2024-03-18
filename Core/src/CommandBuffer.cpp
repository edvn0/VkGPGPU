#include "pch/vkgpgpu_pch.hpp"

#include "CommandBuffer.hpp"

#include "Device.hpp"
#include "Formatters.hpp"
#include "Swapchain.hpp"
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
  CommandBufferProperties props{};
  props.queue_type = type;
  props.count = 1;
  props.is_primary = true;
  props.owned_by_swapchain = false;
  return ImmediateCommandBuffer(device, props);
}

ImmediateCommandBuffer::ImmediateCommandBuffer(const Device &device,
                                               CommandBufferProperties props) {
  command_buffer = make_scope<CommandBuffer>(device, props);
  command_buffer->begin(0);
}

ImmediateCommandBuffer::~ImmediateCommandBuffer() {
  try {
    command_buffer->end_and_submit();
  } catch (const std::exception &exc) {
    error("Failed to submit command buffer. Reason: {}", exc);
  }
}

auto ImmediateCommandBuffer::get_command_buffer() const -> VkCommandBuffer {
  return command_buffer->get_command_buffer();
}

CommandBuffer::CommandBuffer(const Device &dev, CommandBufferProperties props)
    : device(&dev), properties(props) {
  if (properties.queue_type == Queue::Type::Unknown) {
    throw NoQueueTypeException("Unknown queue type");
  }

  supports_device_query =
      props.record_stats &&
      device->check_support(Feature::DeviceQuery, properties.queue_type);

  command_buffers.resize(properties.count);
  query_pools.resize(properties.count);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = *device->get_family_index(properties.queue_type);
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  verify(vkCreateCommandPool(device->get_device(), &pool_info, nullptr,
                             &command_pool),
         "vkCreateCommandPool", "Failed to create command pool");

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  alloc_info.level = properties.is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                           : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  alloc_info.commandBufferCount = static_cast<u32>(command_buffers.size());

  for (auto &command_buffer : command_buffers) {
    verify(vkAllocateCommandBuffers(device->get_device(), &alloc_info,
                                    &command_buffer.command_buffer),
           "vkAllocateCommandBuffers", "Failed to allocate command buffers");
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto i = 0U; i < properties.count; ++i) {
    verify(vkCreateFence(device->get_device(), &fence_info, nullptr,
                         &command_buffers[i].fence),
           "vkCreateFence", "Failed to create fence");
  }

  // Create semaphores
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto i = 0U; i < properties.count; ++i) {
    verify(vkCreateSemaphore(device->get_device(), &semaphore_info, nullptr,
                             &command_buffers[i].finished_semaphore),
           "vkCreateSemaphore", "Failed to create semaphore");
  }

  if (supports_device_query) {
    create_query_objects();
  }
}

CommandBuffer::~CommandBuffer() {
  vkDeviceWaitIdle(device->get_device());
  if (supports_device_query) {
    destroy_query_objects();
  }

  vkDestroyCommandPool(device->get_device(), command_pool, nullptr);

  for (const auto &command_buffer : command_buffers) {
    vkDestroyFence(device->get_device(), command_buffer.fence, nullptr);
  }

  // Destroy semaphores
  for (const auto &command_buffer : command_buffers) {
    vkDestroySemaphore(device->get_device(), command_buffer.finished_semaphore,
                       nullptr);
  }
}

auto CommandBuffer::get_command_buffer() const -> VkCommandBuffer {
  return active_frame->command_buffer;
}

auto CommandBuffer::begin(u32 provided_frame) -> void {

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin(provided_frame, begin_info);
}

auto CommandBuffer::begin(u32 current_frame,
                          VkCommandBufferBeginInfo &begin_info) -> void {
  timestamp_next_available_query = 2;

  active_frame_index = current_frame;
  active_frame = &command_buffers.at(current_frame);

  verify(vkBeginCommandBuffer(get_command_buffer(), &begin_info),
         "vkBeginCommandBuffer", "Failed to begin recording command buffer");
  verify(vkWaitForFences(device->get_device(), 1, &active_frame->fence, VK_TRUE,
                         timeout),
         "vkWaitForFences", "Failed to wait for fence");
  if (supports_device_query) {
    // Timestamp query
    vkCmdResetQueryPool(get_command_buffer(), query_pools[current_frame], 0,
                        timestamp_query_count);
    vkCmdWriteTimestamp(get_command_buffer(),
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        query_pools[current_frame], 0);

    if (properties.queue_type == Queue::Type::Graphics) {
      // Pipeline stats query
      vkCmdResetQueryPool(get_command_buffer(),
                          pipeline_statistics_query_pools[current_frame], 0,
                          pipeline_query_count);
      vkCmdBeginQuery(get_command_buffer(),
                      pipeline_statistics_query_pools[current_frame], 0, 0);
    }
  }
}

auto CommandBuffer::submit() -> void {
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  constexpr VkPipelineStageFlags wait_stage_mask =
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  submit_info.pWaitDstStageMask = &wait_stage_mask;
  // submit_info.signalSemaphoreCount = 1;
  // submit_info.pSignalSemaphores = &active_frame->finished_semaphore;

  submit_info.commandBufferCount = 1;
  std::array<VkCommandBuffer, 1> relevant_buffers{get_command_buffer()};
  submit_info.pCommandBuffers = relevant_buffers.data();

  auto relevant_queue = get_preferred_queue();

  verify(vkWaitForFences(device->get_device(), 1, &active_frame->fence, VK_TRUE,
                         timeout),
         "vkWaitForFences", "Failed to wait for fence");
  verify(vkResetFences(device->get_device(), 1, &active_frame->fence),
         "vkResetFences", "Failed to reset fence");

  if (properties.mutex_around_queue) {
    static std::mutex queue_mutex;
    std::scoped_lock lock{queue_mutex};
    verify(vkQueueSubmit(relevant_queue, 1, &submit_info, active_frame->fence),
           "vkQueueSubmit", "Failed to submit queue");
  } else {
    verify(vkQueueSubmit(relevant_queue, 1, &submit_info, active_frame->fence),
           "vkQueueSubmit", "Failed to submit queue");
  }
  verify(vkWaitForFences(device->get_device(), 1, &active_frame->fence, VK_TRUE,
                         timeout),
         "vkWaitForFences", "Failed to wait for fence");

  if (supports_device_query) {
    // Retrieve timestamp query results
    vkGetQueryPoolResults(device->get_device(), query_pools[active_frame_index],
                          0, timestamp_next_available_query,
                          timestamp_next_available_query * sizeof(uint64_t),
                          timestamp_query_results[active_frame_index].data(),
                          sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    for (u32 i = 0; i < timestamp_next_available_query; i += 2) {
      uint64_t startTime = timestamp_query_results[active_frame_index][i];
      uint64_t endTime = timestamp_query_results[active_frame_index][i + 1];
      float nsTime =
          endTime > startTime
              ? (endTime - startTime) *
                    device->get_device_properties().limits.timestampPeriod
              : 0.0f;
      execution_gpu_times[active_frame_index][i / 2] = nsTime * 0.000001f;
    }

    if (properties.queue_type == Queue::Type::Graphics) {
      vkGetQueryPoolResults(
          device->get_device(),
          pipeline_statistics_query_pools[active_frame_index], 0, 1,
          sizeof(PipelineStatistics),
          &pipeline_statistics_query_results[active_frame_index],
          sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    }
  }

  active_frame = nullptr;
  active_frame_index = 0;
}

auto CommandBuffer::end() -> void {
  if (supports_device_query) {
    vkCmdWriteTimestamp(get_command_buffer(),
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        query_pools[active_frame_index], 1);
    if (properties.queue_type == Queue::Type::Graphics) {
      vkCmdEndQuery(get_command_buffer(),
                    pipeline_statistics_query_pools[active_frame_index], 0);
    }
  }

  verify(vkEndCommandBuffer(get_command_buffer()), "vkEndCommandBuffer",
         "Failed to record command buffer");
}

auto CommandBuffer::end_and_submit() -> void {
  end();
  submit();
}

void CommandBuffer::create_query_objects() {
  VkQueryPoolCreateInfo queryPoolCreateInfo = {};
  queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  queryPoolCreateInfo.pNext = nullptr;

  const u32 maxUserQueries = 16;
  timestamp_query_count = 2 + 2 * maxUserQueries;

  queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  queryPoolCreateInfo.queryCount = timestamp_query_count;
  query_pools.resize(properties.count);
  for (auto &timestampQueryPool : query_pools)
    vkCreateQueryPool(device->get_device(), &queryPoolCreateInfo, nullptr,
                      &timestampQueryPool);

  timestamp_query_results.resize(properties.count);
  for (auto &timestampQueryResults : timestamp_query_results)
    timestampQueryResults.resize(timestamp_query_count);

  execution_gpu_times.resize(properties.count);
  for (auto &executionGPUTimes : execution_gpu_times)
    executionGPUTimes.resize(timestamp_query_count / 2);

  if (properties.queue_type == Queue::Type::Graphics) {
    pipeline_query_count = 7;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    queryPoolCreateInfo.queryCount = pipeline_query_count;
    queryPoolCreateInfo.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

    pipeline_statistics_query_pools.resize(properties.count);
    for (auto &pipelineStatisticsQueryPools : pipeline_statistics_query_pools)
      vkCreateQueryPool(device->get_device(), &queryPoolCreateInfo, nullptr,
                        &pipelineStatisticsQueryPools);

    pipeline_statistics_query_results.resize(properties.count);
  }
}

void CommandBuffer::destroy_query_objects() {
  for (auto i = 0U; i < properties.count; ++i) {
    vkDestroyQueryPool(device->get_device(), query_pools[i], nullptr);

    if (properties.queue_type == Queue::Type::Graphics) {
      vkDestroyQueryPool(device->get_device(),
                         pipeline_statistics_query_pools[i], nullptr);
    }
  }
}

auto CommandBuffer::begin_timestamp_query() -> u32 {
  u32 query_index = timestamp_next_available_query;
  timestamp_next_available_query += 2;
  auto commandBuffer = command_buffers[active_frame_index].command_buffer;
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      query_pools[active_frame_index], query_index);
  return query_index;
}

void CommandBuffer::end_timestamp_query(u32 query_index) {
  auto commandBuffer = command_buffers[active_frame_index].command_buffer;
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      query_pools[active_frame_index], query_index + 1);
}

auto CommandBuffer::get_execution_gpu_time(u32 frame_index,
                                           u32 query_index) const -> float {
  if (query_index == UINT32_MAX ||
      query_index / 2 >= timestamp_next_available_query / 2)
    return 0.0f;

  return execution_gpu_times.at(frame_index).at(query_index / 2);
}

auto CommandBuffer::get_preferred_queue() const -> VkQueue {
  return device->get_queue(properties.queue_type);
}

auto CommandBuffer::construct(const Device &dev, CommandBufferProperties props)
    -> Scope<CommandBuffer> {
  return make_scope<CommandBuffer>(dev, props);
}

SwapchainCommandBuffer::SwapchainCommandBuffer(const Swapchain &swapchain,
                                               CommandBufferProperties props)
    : CommandBuffer{swapchain.get_device(), props}, swapchain{&swapchain} {}

auto SwapchainCommandBuffer::get_command_buffer() const -> VkCommandBuffer {
  return swapchain->get_drawbuffer();
}

} // namespace Core
