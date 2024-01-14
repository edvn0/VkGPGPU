#pragma once

#include <bit>
#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

namespace Core {

struct Colour {
  float r{};
  float g{};
  float b{};
  float a{};
};

class DebugMarker {
public:
  // Get function pointers for the debug report extensions from the device
  static void setup(VkDevice device, VkPhysicalDevice physical_device);

  // Sets the debug name of an object
  // All Objects in Vulkan are represented by their 64-bit handles which are
  // passed into this function along with the object type
  template <typename T>
  static void set_object_name(VkDevice device, T *object,
                              VkDebugReportObjectTypeEXT object_type,
                              const char *name) {
    set_object_name(device, std::bit_cast<std::uint64_t>(object), object_type,
                    name);
  }

  // Sets the debug name of an object
  // All Objects in Vulkan are represented by their 64-bit handles which are
  // passed into this function along with the object type
  static void set_object_name(VkDevice device, uint64_t object,
                              VkDebugReportObjectTypeEXT object_type,
                              const char *name);

  // Set the tag for an object
  template <typename T>
  static void set_object_tag(VkDevice device, T *object,
                             VkDebugReportObjectTypeEXT object_tag,
                             uint64_t name, size_t tag_size, const void *tag) {
    set_object_tag(device, std::bit_cast<std::uint64_t>(object), object_tag,
                   name, tag_size, tag);
  }

  // Set the tag for an object
  static void set_object_tag(VkDevice device, uint64_t object,
                             VkDebugReportObjectTypeEXT object_tag,
                             uint64_t name, size_t tag_size, const void *tag);

  // Start a new debug marker region
  static void begin_region(VkCommandBuffer cmdbuffer, const char *marker_name,
                           Colour color);

  // Insert a new debug marker into the command buffer
  static void insert(VkCommandBuffer cmdbuffer, const std::string &marker_name,
                     Colour color);

  // End the current debug marker region
  static void end_region(VkCommandBuffer buffer);
};
} // namespace Core
