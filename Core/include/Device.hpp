#pragma once

#include "DescriptorResource.hpp"
#include "ImageProperties.hpp"
#include "Instance.hpp"
#include "Types.hpp"

#include <fmt/core.h>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "core/Forward.hpp"

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
  virtual ~Device();

  auto get_device() const -> VkDevice { return device; }
  auto get_physical_device() const -> VkPhysicalDevice {
    return physical_device;
  }

  [[nodiscard]] auto get_family_index(Queue::Type type) const
      -> std::optional<u32> {
    if (!queues.contains(type)) {
      return std::nullopt;
    }

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
  [[nodiscard]] auto get_descriptor_resource() const
      -> const Scope<DescriptorResource> & {
    return descriptor_resource;
  }

  auto get_physical_device_surface_formats(VkSurfaceKHR) const
      -> std::vector<VkSurfaceFormatKHR>;
  auto get_physical_device_surface_present_modes(VkSurfaceKHR) const
      -> std::vector<VkPresentModeKHR>;

  static auto construct(const Instance &, const Window &) -> Scope<Device>;

protected:
  explicit Device(const Instance &, const Window &);

private:
  const Instance &instance;
  VkDevice device{nullptr};
  VkPhysicalDevice physical_device{nullptr};
  Scope<DescriptorResource> descriptor_resource;

  auto construct_vulkan_device(const Window &) -> void;

  struct IndexedQueue {
    u32 family_index{};
    VkQueue queue{nullptr};
  };

  struct QueueFeatureSupport {
    bool timestamping{false};
  };
  std::unordered_map<Queue::Type, IndexedQueue> queues{};
  std::unordered_map<Queue::Type, QueueFeatureSupport> queue_support{};

  static auto enumerate_physical_devices(VkInstance)
      -> std::vector<VkPhysicalDevice>;
  static auto select_physical_device(const std::vector<VkPhysicalDevice> &)
      -> VkPhysicalDevice;
  using IndexQueueTypePair =
      std::tuple<Queue::Type, VkDeviceQueueCreateInfo, bool>;
  static auto find_all_possible_queue_infos(VkPhysicalDevice, VkSurfaceKHR)
      -> std::vector<IndexQueueTypePair>;
  auto create_vulkan_device(VkPhysicalDevice, std::vector<IndexQueueTypePair> &)
      -> VkDevice;
  auto initialize_queues(const std::vector<IndexQueueTypePair> &) -> void;
};

} // namespace Core

template <> struct fmt::formatter<Core::Queue::Type> : formatter<const char *> {
  auto format(const Core::Queue::Type &type, format_context &ctx) const
      -> decltype(ctx.out());
};

template <> struct fmt::formatter<VkResult> : formatter<const char *> {
  auto format(const VkResult &type, format_context &ctx) const
      -> decltype(ctx.out());
};
