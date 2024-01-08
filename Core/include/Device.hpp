#pragma once

#include "Instance.hpp"
#include "Types.hpp"

#include <fmt/core.h>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

namespace Queue {
enum class Type : u8 {
  Graphics,
  Compute,
  Transfer,
  Present,
  Unknown,
};
}

enum class Feature : u8 {
  DeviceQuery,

};

class Device {
public:
  explicit Device(const Instance &);
  ~Device();

  auto get_device() const -> VkDevice { return device; }
  auto get_physical_device() const -> VkPhysicalDevice {
    return physical_device;
  }

  [[nodiscard]] auto get_family_index(Queue::Type type) const -> u32 {
    return queues.at(type).family_index;
  }

  [[nodiscard]] auto get_queue(Queue::Type type) const -> VkQueue {
    return queues.at(type).queue;
  }

  [[nodiscard]] auto
  check_support(Feature feature,
                Queue::Type queue = Queue::Type::Graphics) const -> bool;
  [[nodiscard]] auto get_device_properties() const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    return properties;
  }

  static auto construct(const Instance &) -> Scope<Device>;

private:
  const Instance &instance;
  VkDevice device{nullptr};
  VkPhysicalDevice physical_device{nullptr};

  auto construct_vulkan_device() -> void;

  struct IndexedQueue {
    u32 family_index{};
    VkQueue queue{nullptr};
  };

  struct QueueFeatureSupport {
    bool timestamping{false};
  };
  std::unordered_map<Queue::Type, IndexedQueue> queues{};
  std::unordered_map<Queue::Type, QueueFeatureSupport> queue_support{};
};

} // namespace Core

template <> struct fmt::formatter<Core::Queue::Type> : formatter<const char *> {
  auto format(const Core::Queue::Type &type, format_context &ctx) const
      -> decltype(ctx.out());
};