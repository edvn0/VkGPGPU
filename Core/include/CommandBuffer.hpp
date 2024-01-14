#pragma once

#include "Config.hpp"
#include "Device.hpp"
#include "Types.hpp"

#include <array>
#include <vulkan/vulkan.h>

namespace Core {

class CommandBuffer;
template <typename T, typename... Args>
concept CommandBufferBindable =
    requires(T &object, CommandBuffer &command_buffer, Args &&...args) {
      object.bind(command_buffer, std::forward<Args>(args)...);
    } || requires(T &object, CommandBuffer &command_buffer) {
      object.bind(command_buffer);
    };

class ImmediateCommandBuffer {
public:
  explicit ImmediateCommandBuffer(const Device &device, Queue::Type);
  ~ImmediateCommandBuffer();

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer;

private:
  Scope<CommandBuffer> command_buffer{nullptr};
  Queue::Type type;

  auto begin() -> void;
  auto submit_and_end() -> void;
};

auto create_immediate(const Device &device, Queue::Type = Queue::Type::Graphics)
    -> ImmediateCommandBuffer;

class CommandBuffer {
public:
  enum class Type : u8 { Compute, Graphics, Transfer };

  explicit CommandBuffer(const Device &, Type type, u32 = Config::frame_count);
  ~CommandBuffer();

  auto begin(u32 current_frame, Queue::Type) -> void;
  auto end() const -> void;

  /**
   * @brief Ends the command buffer and submits it to the queue.
   *
   * @param type The queue type to submit to.
   */
  auto end_and_submit(Queue::Type type) const -> void;

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer {
    return active_frame->command_buffer;
  }

  template <class T> void bind(T &object) { object.bind(*this); }

private:
  auto submit(Queue::Type) const -> void;
  const Device &device;
  u32 frame_count{Config::frame_count};
  bool supports_device_query{false};

  struct FrameCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkFence compute_in_flight_fence{};
    VkFence graphics_in_flight_fence{};
    VkSemaphore compute_finished_semaphore{};
    VkSemaphore image_available_semaphore{};
    VkSemaphore render_finished_semaphore{};
    VkQueryPool query_pool{};
  };
  FrameCommandBuffer *active_frame{nullptr};
  std::array<FrameCommandBuffer, Config::frame_count> command_buffers{};

  Queue::Type queue_type{Queue::Type::Unknown};
  VkCommandPool command_pool{};

  void create_query_objects();
  void destroy_query_objects() const;
};

} // namespace Core
