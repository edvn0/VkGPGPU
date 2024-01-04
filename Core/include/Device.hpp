#pragma once

#include "Types.hpp"
#include <vulkan/vulkan.h>

namespace Core {

class Device {
  using Ptr = const Device *const;

public:
  [[nodiscard]] static auto get() -> Ptr;

  auto get_device() const -> VkDevice { return device; }
  auto get_physical_device() const -> VkPhysicalDevice {
    return physical_device;
  }

private:
  VkDevice device{nullptr};
  VkPhysicalDevice physical_device{nullptr};

  static auto construct_device() -> Scope<Device>;
  auto construct_vulkan_device() -> void;

  static inline Scope<Device> static_device{nullptr};
};

} // namespace Core