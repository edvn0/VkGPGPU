#include "pch/vkgpgpu_pch.hpp"

#include "Device.hpp"

#include "Allocator.hpp"
#include "DescriptorResource.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <fmt/format.h>
#include <fmt/std.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Core {

class QueueUnknownException : public BaseException {
public:
  using BaseException::BaseException;
};

Device::Device(const Instance &inst) : instance(inst) {
  construct_vulkan_device();
  descriptor_resource = DescriptorResource::construct(*this);
}

Device::~Device() {
  descriptor_resource.reset();

  vkDeviceWaitIdle(device);

  vkDestroyDevice(device, nullptr);

  info("Destroyed Device!");
}

auto Device::check_support(const Feature feature, Queue::Type queue) const
    -> bool {
  if (!queue_support.contains(queue)) {
    error("Unknown queue type: {}", queue);
    throw QueueUnknownException("Unknown queue type");
  }

  auto physical_device_features = VkPhysicalDeviceFeatures{};
  vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

  if (feature == Feature::DeviceQuery) {
    return queue_support.at(queue).timestamping;
  }

  return false;
}

auto Device::construct(const Instance &instance) -> Scope<Device> {
  return make_scope<Device>(instance);
}

auto Device::enumerate_physical_devices(VkInstance inst)
    -> std::vector<VkPhysicalDevice> {
  u32 device_count = 0;
  vkEnumeratePhysicalDevices(inst, &device_count, nullptr);
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(inst, &device_count, devices.data());
  return devices;
}

auto Device::select_physical_device(
    const std::vector<VkPhysicalDevice> &devices) -> VkPhysicalDevice {
  // Prefer devices with discrete GPUs
  for (const auto &dev : devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(dev, &properties);
    // We also want to make sure that the device supports Vulkan 1.2
    if (properties.apiVersion < VK_API_VERSION_1_2) {
      continue;
    }

    // We also want to make sure that the device supports the extensions we
    // need
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extension_count,
                                         nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extension_count,
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
    vkGetPhysicalDeviceFeatures(dev, &features);

    if (!features.samplerAnisotropy) {
      continue;
    }

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      return dev;
      info("Selected device: {}", properties.deviceName);
      break;
    }
  }

  return devices[0];
}

auto Device::find_all_possible_queue_infos(VkPhysicalDevice dev)
    -> std::vector<IndexQueueTypePair> {
  u32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count,
                                           queue_families.data());

  std::vector<IndexQueueTypePair> queue_infos;

  int dedicated_compute_queue_index = -1;
  int dedicated_transfer_queue_index = -1;

  // First pass: Look for dedicated compute and transfer queues
  for (auto i = 0ULL; i < queue_families.size(); ++i) {
    const auto &queue_family = queue_families[i];

    bool is_graphics_queue = queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    bool is_compute_queue = queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT;
    bool is_transfer_queue = queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT;

    if (is_compute_queue && !is_graphics_queue) { // Dedicated compute queue
      dedicated_compute_queue_index = static_cast<i32>(i);
    }

    if (is_transfer_queue && !is_graphics_queue &&
        !is_compute_queue) { // Dedicated transfer queue
      dedicated_transfer_queue_index = static_cast<i32>(i);
    }
  }

  // Second pass: Create queue infos
  for (u32 i = 0; i < queue_families.size(); ++i) {
    const auto &queue_family = queue_families[i];

    if (i == dedicated_compute_queue_index) {
      auto priority = 1.0F;
      VkDeviceQueueCreateInfo queue_info{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = i,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      };
      queue_infos.emplace_back(Queue::Type::Compute, queue_info,
                               queue_family.timestampValidBits > 0);
      continue;
    }

    if (i == dedicated_transfer_queue_index) {
      auto priority = 1.0F;
      VkDeviceQueueCreateInfo queue_info{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = i,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      };
      queue_infos.emplace_back(Queue::Type::Transfer, queue_info,
                               queue_family.timestampValidBits > 0);
      continue;
    }

    if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      auto priority = 0.1F;
      VkDeviceQueueCreateInfo queue_info{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = i,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      };
      queue_infos.emplace_back(Queue::Type::Graphics, queue_info,
                               queue_family.timestampValidBits > 0);
    }
  }

  return queue_infos;
}

auto Device::create_vulkan_device(
    VkPhysicalDevice dev,
    std::vector<IndexQueueTypePair> &index_queue_type_pairs) -> VkDevice {

  VkPhysicalDeviceFeatures device_features{};
  device_features.pipelineStatisticsQuery = VK_TRUE;

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

  VkDevice temp{};
  verify(vkCreateDevice(physical_device, &create_info, nullptr, &temp),
         "vkCreateDevice", "Failed to create Vulkan device");
  return temp;
}

auto Device::initialize_queues(
    const std::vector<IndexQueueTypePair> &index_queue_type_pairs) -> void {
  auto &queue_map = queues;
  for (const auto &[type, queue_info, supports_timestamping] :
       index_queue_type_pairs) {
    VkQueue queue;
    vkGetDeviceQueue(device, queue_info.queueFamilyIndex, 0, &queue);
    IndexedQueue indexed_queue{
        .family_index = queue_info.queueFamilyIndex,
        .queue = queue,
    };
    queue_map[type] = indexed_queue;
  }
}

auto Device::construct_vulkan_device() -> void {

  auto vk_instance = instance.get_instance();
  auto devices = enumerate_physical_devices(vk_instance);
  physical_device = select_physical_device(devices);

  auto index_queue_type_pairs = find_all_possible_queue_infos(physical_device);
  device = create_vulkan_device(physical_device, index_queue_type_pairs);
  initialize_queues(index_queue_type_pairs);

  info("Created Vulkan device with {} queue(s)", queues.size());
  for (auto &&[k, v] : queues) {
    static constexpr auto as_string = [](Queue::Type type) {
      switch (type) {
        using enum Queue::Type;
      case Graphics:
        return "Graphics";
      case Compute:
        return "Compute";
      case Transfer:
        return "Transfer";
      case Present:
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
