#pragma once

#include "CommandBuffer.hpp"
#include "Types.hpp"

#include <functional>
#include <queue>
#include <vulkan/vulkan.h>

#include "core/Forward.hpp"

namespace Core {

class InterfaceSystem {
public:
  InterfaceSystem(const Device &dev, const Window &win, const Swapchain &swap);
  ~InterfaceSystem();

  auto begin_frame() -> void;
  auto end_frame() -> void;

  template <class Func>
    requires requires(Func f, const CommandBuffer &exec) {
      { f.operator()(exec) } -> std::same_as<void>;
    }
  static void on_frame_end(Func &&func) {
    frame_end_callbacks.push(std::forward<Func>(func));
  }

private:
  const Device *device{nullptr};
  const Window *window{nullptr};
  const Swapchain *swapchain{nullptr};

  VkDescriptorPool pool{};

  Scope<CommandBuffer> command_executor;

  using FrameEndCallback = std::function<void(const CommandBuffer &)>;
  static inline std::queue<FrameEndCallback> frame_end_callbacks{};
};

} // namespace Core