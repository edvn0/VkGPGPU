#pragma once

#include "Config.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "Types.hpp"

#include <array>
#include <vulkan/vulkan.h>

#include "core/Forward.hpp"

namespace Core {

struct PipelineStatistics {
  u64 input_assembly_vertices{0};
  u64 input_assembly_primitives{0};
  u64 vs_invocations{0};
  u64 clip_invocations{0};
  u64 clip_primitives{0};
  u64 fs_invocations{0};
  u64 cs_invocations{0};
};

class CommandBuffer;
template <typename T, typename... Args>
concept CommandBufferBindable =
    requires(T &object, CommandBuffer &command_buffer, Args &&...args) {
      object.bind(command_buffer, std::forward<Args>(args)...);
    } || requires(T &object, CommandBuffer &command_buffer) {
      object.bind(command_buffer);
    };

struct CommandBufferProperties {
  Queue::Type queue_type{Queue::Type::Graphics};
  u32 count{Config::frame_count};
  bool is_primary{true};
  bool owned_by_swapchain{false};
  bool record_stats{false};
};

class ImmediateCommandBuffer {
public:
  explicit ImmediateCommandBuffer(const Device &device,
                                  CommandBufferProperties);
  ~ImmediateCommandBuffer();

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer;

private:
  Scope<CommandBuffer> command_buffer{nullptr};

  auto begin() -> void;
  auto submit_and_end() -> void;
};

auto create_immediate(const Device &device, Queue::Type queue_type)
    -> ImmediateCommandBuffer;

class CommandBuffer {
public:
  enum class Type : u8 { Compute, Graphics, Transfer };

  explicit CommandBuffer(const Device &, CommandBufferProperties props);
  virtual ~CommandBuffer();

  auto begin(u32 current_frame) -> void;
  auto begin(u32 current_frame, VkCommandBufferBeginInfo &) -> void;

  auto end() -> void;
  auto submit() -> void;
  auto end_and_submit() -> void;

  [[nodiscard]] virtual auto get_command_buffer() const -> VkCommandBuffer;
  [[nodiscard]] auto get_preferred_queue() const -> VkQueue;

  [[nodiscard]] auto get_statistics() const -> std::tuple<floating> {
    return compute_times.peek();
  }

  auto get_pipeline_statistics(u32 frame_index) const
      -> const PipelineStatistics & {
    return pipeline_statistics_query_results[frame_index];
  }

  auto begin_timestamp_query() -> u32;
  void end_timestamp_query(u32 query_index);
  auto get_execution_gpu_time(u32 frame_index, u32 query_index = 0) const
      -> float;

  template <class T> void bind(T &object) { object.bind(*this); }

  static auto construct(const Device &, CommandBufferProperties)
      -> Scope<CommandBuffer>;

private:
  const Device *device;
  CommandBufferProperties properties{};
  bool supports_device_query{false};

  struct FrameCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkFence fence{};
    VkSemaphore finished_semaphore{};
  };
  FrameCommandBuffer *active_frame{nullptr};
  u32 active_frame_index{0};
  std::vector<FrameCommandBuffer> command_buffers{};

  VkCommandPool command_pool{};

  u32 timestamp_query_count = 0;
  u32 timestamp_next_available_query = 2;
  std::vector<VkQueryPool> query_pools{};
  std::vector<VkQueryPool> pipeline_statistics_query_pools;
  std::vector<std::vector<uint64_t>> timestamp_query_results;
  std::vector<std::vector<float>> execution_gpu_times;

  u32 pipeline_query_count = 0;
  std::vector<PipelineStatistics> pipeline_statistics_query_results;

  Container::CircularBuffer<floating> compute_times{usize{200}};

  void create_query_objects();
  void destroy_query_objects();
};

class SwapchainCommandBuffer : public CommandBuffer {
public:
  SwapchainCommandBuffer(const Swapchain &swapchain, CommandBufferProperties);
  ~SwapchainCommandBuffer() override = default;

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer override;

private:
  const Swapchain *swapchain{nullptr};
};

} // namespace Core
