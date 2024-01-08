#include "pch/vkgpgpu_pch.hpp"

#include "Allocator.hpp"
#include "Device.hpp"
#include "Logger.hpp"
#include "Types.hpp"

#include "Instance.hpp"
#include "Verify.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

#include <fmt/format.h>
#include <fmt/std.h>

namespace Core {

auto Device::get() -> Ptr {
  if (static_device == nullptr) {
    static_device = construct_device();
  }

  return static_device.get();
}

Device::~Device() {

  if (device != nullptr) {
    vkDestroyDevice(device, nullptr);
    info("Destroyed Device!");
  }

  Core::Allocator::destroy();
}

auto Device::check_support(const Feature feature, Queue::Type queue) const -> bool {
  if (!queue_support.contains(queue)) {
    error("Unknown queue type: {}", queue);
    throw std::runtime_error("Unknown queue type");
  }

  auto physical_device_features = VkPhysicalDeviceFeatures{};
  vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

  if (feature == Feature::DeviceQuery) {
    return queue_support.at(queue).timestamping;
  }

  return false;
}

auto Device::construct_device() -> Scope<Device> {
  auto created = make_scope<Device>();
  created->construct_vulkan_device();
  return created;
}

auto Device::construct_vulkan_device() -> void {
  auto instance = Instance::get();
  auto vk_instance = instance->get_instance();

  u32 device_count = 0;
  vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(vk_instance, &device_count, devices.data());

  for (const auto &device : devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    debug("Found device: {}", properties.deviceName);
  }

  // Prefer devices with discrete GPUs
  for (const auto &device : devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    // We also want to make sure that the device supports Vulkan 1.2
    if (properties.apiVersion < VK_API_VERSION_1_2) {
      continue;
    }

    // We also want to make sure that the device supports the extensions we
    // need
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         available_extensions.data());

    std::vector<std::string> required_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool has_all_extensions = true;
    for (const auto &required_extension : required_extensions) {
      bool has_extension = false;
      for (const auto &available_extension : available_extensions) {
        if (required_extension == available_extension.extensionName) {
          has_extension = true;
          break;
        }
      }

      if (!has_extension) {
        has_all_extensions = false;
        break;
      }
    }

    if (!has_all_extensions) {
      continue;
    }

    // We also want to make sure that the device supports the features we need
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    if (!features.samplerAnisotropy) {
      continue;
    }

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      physical_device = device;
      info("Selected device: {}", properties.deviceName);
      break;
    }
  }

  auto find_all_possible_queue_infos = [](VkPhysicalDevice device) {
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families.data());

    using IndexQueueTypePair =
        std::tuple<Queue::Type, VkDeviceQueueCreateInfo, bool>;
    std::vector<IndexQueueTypePair> queue_infos;
    for (u32 i = 0; i < queue_families.size(); ++i) {
      const auto &queue_family = queue_families[i];
      // Is there a unique compute queue?
      bool already_found = false;
      if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        auto priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        queue_infos.emplace_back(Queue::Type::Compute, queue_info,
                                 queue_family.timestampValidBits > 0);
        already_found = true;
      }

      // Is there a unique graphics queue?
      if (!already_found && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        auto priority = 0.1F;
        VkDeviceQueueCreateInfo queue_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        queue_infos.emplace_back(Queue::Type::Graphics, queue_info,
                                 queue_family.timestampValidBits > 0);
      }
    }

    return queue_infos;
  };

  auto index_queue_type_pairs = find_all_possible_queue_infos(physical_device);

  VkPhysicalDeviceFeatures device_features{};

  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (auto &&[type, queue_info, supports_timestamping] :
       index_queue_type_pairs) {
    float priority = 1.0F;
    queue_info.pQueuePriorities = &priority;
    queue_infos.push_back(queue_info);
    queue_support[type] = {supports_timestamping};
  }
  VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .queueCreateInfoCount = static_cast<u32>(queue_infos.size()),
      .pQueueCreateInfos = queue_infos.data(),
      .pEnabledFeatures = &device_features,
  };

  verify(vkCreateDevice(physical_device, &create_info, nullptr, &device),
         "vkCreateDevice", "Failed to create Vulkan device");

  auto &queue_map = queues;
  for (auto &&[type, queue_info, supports_timestamping] :
       index_queue_type_pairs) {
    VkQueue queue;
    vkGetDeviceQueue(device, queue_info.queueFamilyIndex, 0, &queue);
    IndexedQueue indexed_queue{
        .family_index = queue_info.queueFamilyIndex,
        .queue = queue,
    };
    queue_map[type] = indexed_queue;
  }

  info("Created Vulkan device with {} queue(s)", queue_infos.size());
  for (auto &&[k, v] : queue_map) {
    static constexpr auto as_string = [](Queue::Type type) {
      switch (type) {
      case Queue::Type::Graphics:
        return "Graphics";
      case Queue::Type::Compute:
        return "Compute";
      case Queue::Type::Transfer:
        return "Transfer";
      case Queue::Type::Present:
        return "Present";
      default:
        return "Unknown";
      }
    };

    const auto type_string = as_string(k);

    info("{} queue: family index {}, queue {}", type_string, v.family_index,
         fmt::ptr(v.queue));
  }
}

} // namespace Core