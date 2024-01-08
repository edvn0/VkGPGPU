#include "pch/vkgpgpu_pch.hpp"

#include "Environment.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <vector>

namespace Core {

Instance::Instance() { construct_vulkan_instance(); }

Instance::~Instance() {
  vkDestroyInstance(instance, nullptr);
  info("Destroyed Instance!");
}

auto Instance::construct() -> Scope<Instance> { return make_scope<Instance>(); }

auto Instance::construct_vulkan_instance() -> void {
  VkApplicationInfo application_info{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "Vulkan Application",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  // Add layers and extensions as needed
  std::vector<const char *> enabled_layers = {};
  if (Environment::get("ENABLE_VALIDATION_LAYERS")) {
    enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
  }

  std::vector<const char *> enabled_extensions = {};

  VkInstanceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &application_info,
      .enabledLayerCount = static_cast<std::uint32_t>(enabled_layers.size()),
      .ppEnabledLayerNames = enabled_layers.data(),
      .enabledExtensionCount =
          static_cast<std::uint32_t>(enabled_extensions.size()),
      .ppEnabledExtensionNames = enabled_extensions.data(),
  };

  verify(vkCreateInstance(&create_info, nullptr, &instance), "vkCreateInstance",
         "Failed to create Vulkan instance");

  info("Created Vulkan instance. Enabled layers (count={}): [{}], enabled "
       "extensions (count={}): "
       "[{}]",
       enabled_layers.size(), fmt::join(enabled_layers, ", "),
       enabled_extensions.size(), fmt::join(enabled_extensions, ", "));
}

} // namespace Core
