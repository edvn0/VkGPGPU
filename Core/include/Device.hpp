#pragma once

#include "Types.hpp"
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

namespace Queue {
enum class Type :u8{
  Graphics,
  Compute,
  Transfer,
  Present,
  Unknown
};
}

class Device {
  using Ptr = const Device *const;

public:
  [[nodiscard]] static auto get() -> Ptr;

  auto get_device() const -> VkDevice { return device; }
  auto get_physical_device() const -> VkPhysicalDevice {
    return physical_device;
  }

  [[nodiscard]] auto get_family_index(Queue::Type type)const -> u32 {
    return queues.at(type).family_index;
  }

  [[nodiscard]] auto get_queue(Queue::Type type) const -> VkQueue {
    return queues.at(type).queue;
  }

private:
  VkDevice device{nullptr};
  VkPhysicalDevice physical_device{nullptr};

  static auto construct_device() -> Scope<Device>;
  auto construct_vulkan_device() -> void;

  struct IndexedQueue {
    u32 family_index{};
    VkQueue queue{nullptr};
  };

  std::unordered_map<Queue::Type, IndexedQueue> queues{};

  static inline Scope<Device> static_device{nullptr};
};

} // namespace Core