#include "Instance.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Environment.hpp"
#include "Logger.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <set>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "GLFW/glfw3.h"

namespace Core {

Instance::Instance(bool headless) { construct_vulkan_instance(headless); }

Instance::~Instance() {
  vkDestroyInstance(instance, nullptr);
  info("Destroyed Instance!");
}

auto Instance::construct() -> Scope<Instance> {
  return make_scope<Instance>(false);
}

auto Instance::construct_headless() -> Scope<Instance> {
  return make_scope<Instance>(true);
}

auto Instance::construct_vulkan_instance(bool headless) -> void {
  glfwInit();

  VkApplicationInfo application_info{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "VkGPGPU",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  const auto use_graphical_interface = !headless;
  std::vector<const char *> enabled_layers = {};
  std::vector<const char *> enabled_extensions = {};
  if (Environment::get("ENABLE_VALIDATION_LAYERS")) {
    enabled_layers.emplace_back("VK_LAYER_KHRONOS_validation");
  }
  if (use_graphical_interface) {
    u32 glfw_count{0};
    glfwGetRequiredInstanceExtensions(&glfw_count);
    const auto *glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_count);

    enabled_extensions =
        std::vector(glfw_extensions, glfw_extensions + glfw_count);
  }

  const VkInstanceCreateInfo create_info{
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
