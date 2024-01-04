#pragma once

#include "Device.hpp"
#include "Types.hpp"

#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

class CommandBuffer {
public:
  enum class Type : u8 { Compute, Graphics };

  explicit CommandBuffer(u32 frame_count, Type type);
  ~CommandBuffer();

  auto begin(u32 current_frame) -> void;
  auto end() -> void;
  auto submit() -> void;
  auto submit_and_end() -> void;

  template <class T>
    requires requires(T &object, CommandBuffer &command_buffer) {
               object.bind(command_buffer);
             }
  void bind(T &object) {
    object.bind(*this);
  }

  [[nodiscard]] auto get_command_buffer() const -> VkCommandBuffer {
    return command_buffers.at(current_frame).command_buffer;
  }

private:
  VkCommandPool command_pool{};
  i32 current_frame{-1};
  Queue::Type queue_type{Queue::Type::Unknown};

  struct FrameCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkFence fence{};
  };

  std::vector<FrameCommandBuffer> command_buffers{};
};

} // namespace Core