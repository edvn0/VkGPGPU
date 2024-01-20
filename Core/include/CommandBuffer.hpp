#pragma once

#include "Config.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "Types.hpp"

#include <array>
#include <vulkan/vulkan.h>

#include "core/Forward.hpp"

namespace Core {

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
  ~CommandBuffer();

  auto begin(u32 current_frame) -> void;
  auto begin(u32 current_frame, VkCommandBufferBeginInfo &) -> void;

  auto end() -> void;
  auto end_and_submit() -> void;

  [[nodiscard]] virtual auto get_command_buffer() const -> VkCommandBuffer;
  [[nodiscard]] auto get_preferred_queue() const -> VkQueue;

  [[nodiscard]] auto get_statistics() const -> std::tuple<floating> {
    return compute_times.peek();
  }

  template <class T> void bind(T &object) { object.bind(*this); }

  static auto construct(const Device &, CommandBufferProperties)
      -> Scope<CommandBuffer>;

private:
  auto submit() -> void;
  const Device &device;
  CommandBufferProperties properties{};
  bool supports_device_query{false};

  struct FrameCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkFence fence{};
    VkSemaphore finished_semaphore{};
  };
  FrameCommandBuffer *active_frame{nullptr};
  VkQueryPool *active_pool{nullptr};
  std::vector<FrameCommandBuffer> command_buffers{};

  VkCommandPool command_pool{};

  std::vector<VkQueryPool> query_pools{};

  Container::CircularBuffer<floating> compute_times;

  void create_query_objects();
  void destroy_query_objects();
};

class SwapchainCommandBuffer : public CommandBuffer {
public:
  SwapchainCommandBuffer(const Swapchain &swapchain, CommandBufferProperties);

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer override;

private:
  const Swapchain *swapchain{nullptr};
};

} // namespace Core
