#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "Types.hpp"
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace Core {

class DescriptorMap {
public:
  DescriptorMap();
  ~DescriptorMap();

  auto add_for_frames(u32 set, u32 binding, const Buffer&info)
      -> void;

  auto bind(CommandBuffer &buffer, u32 frame, VkPipelineLayout binding) -> void;

  [[nodiscard]] auto get_descriptor_pool() -> VkDescriptorPool;
  [[nodiscard]] auto get_descriptor_set_layout() -> VkDescriptorSetLayout {
    return m_descriptor_set_layout;
  }

private:
  std::vector<VkDescriptorSet> m_descriptors{};

  VkDescriptorPool m_descriptor_pool{};
  VkDescriptorSetLayout m_descriptor_set_layout{};
};

} // namespace Core