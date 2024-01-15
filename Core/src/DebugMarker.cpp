#include "pch/vkgpgpu_pch.hpp"

#include "DebugMarker.hpp"

#include "Logger.hpp"

#include <cstring>
#include <span>
#include <vector>

namespace Core {

static bool active = false;
static bool extension_present = false;

PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag =
    VK_NULL_HANDLE; // NOLINT
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName =
    VK_NULL_HANDLE;                                                    // NOLINT
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin = VK_NULL_HANDLE;   // NOLINT
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd = VK_NULL_HANDLE;       // NOLINT
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert = VK_NULL_HANDLE; // NOLINT

void DebugMarker::setup(const Device &device,
                        VkPhysicalDevice physical_device) {
  uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                       &extension_count, nullptr);
  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                       &extension_count, extensions.data());
  for (const auto &extension : extensions) {
    std::span ext_name = extension.extensionName;
    std::string extension_name{ext_name.data()};

    if (extension_name == VK_EXT_DEBUG_MARKER_EXTENSION_NAME) {
      extension_present = true;
      break;
    }
  }

  if (extension_present) {
    vkDebugMarkerSetObjectTag =
        std::bit_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(
            device.get_device(), "vkDebugMarkerSetObjectTagEXT"));
    vkDebugMarkerSetObjectName =
        std::bit_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(
            device.get_device(), "vkDebugMarkerSetObjectNameEXT"));
    vkCmdDebugMarkerBegin = std::bit_cast<PFN_vkCmdDebugMarkerBeginEXT>(
        vkGetDeviceProcAddr(device.get_device(), "vkCmdDebugMarkerBeginEXT"));
    vkCmdDebugMarkerEnd = std::bit_cast<PFN_vkCmdDebugMarkerEndEXT>(
        vkGetDeviceProcAddr(device.get_device(), "vkCmdDebugMarkerEndEXT"));
    vkCmdDebugMarkerInsert = std::bit_cast<PFN_vkCmdDebugMarkerInsertEXT>(
        vkGetDeviceProcAddr(device.get_device(), "vkCmdDebugMarkerInsertEXT"));
    // Set flag if at least one function pointer is present
    active = (vkDebugMarkerSetObjectName != VK_NULL_HANDLE);

    info("Debug marker extension for Vulkan was present.");
  } else {
    error("Debug marker extension for Vulkan was not present.");
  }
}

void DebugMarker::set_object_name(const Device &device, uint64_t object,
                                  VkDebugReportObjectTypeEXT object_type,
                                  const char *name) {
  if (active) {
    VkDebugMarkerObjectNameInfoEXT name_info{};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    name_info.objectType = object_type;
    name_info.object = object;
    name_info.pObjectName = name;
    vkDebugMarkerSetObjectName(device.get_device(), &name_info);
  }
}

void DebugMarker::set_object_tag(const Device &device, uint64_t object,
                                 VkDebugReportObjectTypeEXT object_tag,
                                 uint64_t name, size_t tag_size,
                                 const void *tag) {
  if (active) {
    VkDebugMarkerObjectTagInfoEXT tag_info{};
    tag_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
    tag_info.objectType = object_tag;
    tag_info.object = object;
    tag_info.tagName = name;
    tag_info.tagSize = tag_size;
    tag_info.pTag = tag;
    vkDebugMarkerSetObjectTag(device.get_device(), &tag_info);
  }
}

void DebugMarker::begin_region(VkCommandBuffer cmdbuffer,
                               const char *marker_name, Colour color) {
  if (active) {
    VkDebugMarkerMarkerInfoEXT marker_info{};
    marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    std::span colour_span = marker_info.color;
    std::memcpy(colour_span.data(), &color, sizeof(float) * 4);
    marker_info.pMarkerName = marker_name;
    vkCmdDebugMarkerBegin(cmdbuffer, &marker_info);
  }
}

void DebugMarker::insert(VkCommandBuffer cmdbuffer,
                         const std::string &marker_name, Colour color) {
  if (active) {
    VkDebugMarkerMarkerInfoEXT marker_info{};
    marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    std::span colour_span = marker_info.color;
    std::memcpy(colour_span.data(), &color, sizeof(float) * 4);
    marker_info.pMarkerName = marker_name.c_str();
    vkCmdDebugMarkerInsert(cmdbuffer, &marker_info);
  }
}

void DebugMarker::end_region(VkCommandBuffer buffer) {
  if (active) {
    vkCmdDebugMarkerEnd(buffer);
  }
}

} // namespace Core
