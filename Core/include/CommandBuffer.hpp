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
  explicit ImmediateCommandBuffer(const Device &device);
  ~ImmediateCommandBuffer();

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer;

private:
  Scope<CommandBuffer> command_buffer{nullptr};

  auto begin() -> void;
  auto submit_and_end() -> void;
};

auto create_immediate(const Device &device) -> ImmediateCommandBuffer;

class CommandBuffer {
public:
  enum class Type : u8 { Compute, Graphics };

  explicit CommandBuffer(const Device &, Type type, u32 = Config::frame_count);
  ~CommandBuffer();

  auto begin(u32 current_frame) -> void;
  auto end() -> void;
  auto end_and_submit() -> void;

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer {
    return active_frame->command_buffer;
  }

  template <class T> void bind(T &object) { object.bind(*this); }

private:
  auto submit() -> void;
  const Device &device;
  bool supports_device_query{false};
  u32 frame_count{Config::frame_count};

  struct FrameCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkFence fence{};
    VkSemaphore finished_semaphore{};
  };
  FrameCommandBuffer *active_frame{nullptr};
  VkQueryPool *active_pool{nullptr};
  std::array<FrameCommandBuffer, Config::frame_count> command_buffers{};

  Queue::Type queue_type{Queue::Type::Unknown};
  VkCommandPool command_pool{};

  std::array<VkQueryPool, Config::frame_count> query_pools{};

  void create_query_objects();
  void destroy_query_objects();
};

} // namespace Core
