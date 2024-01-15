#pragma once

#include "Config.hpp"
#include "Device.hpp"

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Core {

class DescriptorResource {
public:
  ~DescriptorResource();

  [[nodiscard]] auto
  allocate_descriptor_set(const VkDescriptorSetAllocateInfo &alloc_info) const
      -> VkDescriptorSet;
  [[nodiscard]] auto allocate_many_descriptor_sets(
      const VkDescriptorSetAllocateInfo &alloc_info) const
      -> std::vector<VkDescriptorSet>;

  void begin_frame(u32 frame);
  void end_frame();

  static auto construct(const Device &) -> Scope<DescriptorResource>;

private:
  explicit DescriptorResource(const Device &);

  void create_pool();
  void handle_fragmentation() const;
  void handle_out_of_memory() const;

  const Device *device;
  u32 current_frame{0};
  std::array<VkDescriptorPool, Config::frame_count> descriptor_pools;
  std::array<VkDescriptorPoolSize, 11> pool_sizes;

  // Optional: Structure to keep track of allocated descriptor sets and their
  // usage
};

} // namespace Core
