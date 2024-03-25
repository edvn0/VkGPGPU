#include "pch/vkgpgpu_pch.hpp"

#include "Instance.hpp"

#include "Environment.hpp"
#include "Logger.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <vector>

namespace Core {

VkResult create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void destroy_debug_utils_messenger_ext(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *) {
  if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    error("Validation layer: {}", callback_data->pMessage);
  } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    info("Validation layer: {}", callback_data->pMessage);
  } else if (message_severity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    debug("Validation layer: {}", callback_data->pMessage);
  }

  return VK_FALSE;
}

void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debug_callback;
}

Instance::Instance(bool headless) { construct_vulkan_instance(headless); }

Instance::~Instance() {
  if (enable_validation_layers) {
    destroy_debug_utils_messenger_ext(instance, debug_messenger, nullptr);
    info("Destroyed Debug Messenger!");
  }

  vkDestroyInstance(instance, nullptr);
  info("Destroyed Instance!");
}

auto Instance::construct(bool headless) -> Scope<Instance> {
  return Scope<Instance>{new Instance{headless}};
}

auto Instance::construct_vulkan_instance(bool headless) -> void {
  VkApplicationInfo application_info{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "Vulkan Application",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };

  // Add layers and extensions as needed
  std::vector<const char *> enabled_layers = {};
  enable_validation_layers =
      Environment::get("ENABLE_VALIDATION_LAYERS").has_value();
  if (enable_validation_layers) {
    enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
  }

  std::vector<const char *> enabled_extensions = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
#ifndef GPGPU_DEBUG
  enabled_extensions.clear();
#endif

  if (!headless) {
    if (!glfwInit()) {
      error("Failed to initialize GLFW");
      throw std::runtime_error("Failed to initialize GLFW");
    }

    u32 count;
    glfwGetRequiredInstanceExtensions(&count);
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

    for (u32 i = 0; i < count; ++i) {
      enabled_extensions.push_back(glfw_extensions[i]);
    }
  }
  VkInstanceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &application_info,
      .enabledLayerCount = static_cast<u32>(enabled_layers.size()),
      .ppEnabledLayerNames = enabled_layers.data(),
      .enabledExtensionCount = static_cast<u32>(enabled_extensions.size()),
      .ppEnabledExtensionNames = enabled_extensions.data(),
  };

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
  VkValidationFeaturesEXT validation_features{};
  validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  if (enable_validation_layers) {
    std::array<VkValidationFeatureEnableEXT, 1> enabled_features{};
    // enabled_features[0] =
    //    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;

    validation_features.enabledValidationFeatureCount = enabled_features.size();
    validation_features.pEnabledValidationFeatures = enabled_features.data();
    populate_debug_messenger_create_info(debug_create_info);
    create_info.pNext = &debug_create_info;
    debug_create_info.pNext = &validation_features;
  }

  verify(vkCreateInstance(&create_info, nullptr, &instance), "vkCreateInstance",
         "Failed to create Vulkan instance");

  info("Created Vulkan instance. Enabled layers (count={}): [{}], enabled "
       "extensions (count={}): "
       "[{}]",
       enabled_layers.size(), fmt::join(enabled_layers, ", "),
       enabled_extensions.size(), fmt::join(enabled_extensions, ", "));

  setup_debug_messenger();
}

auto Instance::setup_debug_messenger() -> void {
  if (!enable_validation_layers)
    return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  populate_debug_messenger_create_info(createInfo);

  verify(create_debug_utils_messenger_ext(instance, &createInfo, nullptr,
                                          &debug_messenger),
         "create_debug_utils_messenger_ext",
         "Failed to set up debug messenger!");
}

} // namespace Core
