#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "Types.hpp"
#include <unordered_map>
#include <vulkan/vulkan.h>

template <> struct fmt::formatter<VkDescriptorSet> : formatter<const char *> {
  auto format(const VkDescriptorSet &type, format_context &ctx) const
      -> decltype(ctx.out());
};

namespace Core {

class DescriptorMap {
  using DescriptorSets = std::vector<VkDescriptorSet>;

public:
  DescriptorMap();
  ~DescriptorMap();

  auto add_for_frames(u32 set, u32 binding, const Buffer &info) -> void;

  auto bind(CommandBuffer &buffer, u32 frame, VkPipelineLayout binding) -> void;

  [[nodiscard]] auto get_descriptor_pool() -> VkDescriptorPool;
  [[nodiscard]] auto get_descriptor_set_layout() -> VkDescriptorSetLayout {
    return descriptor_set_layout;
  }

  auto descriptors() -> std::unordered_map<u32, DescriptorSets> &;

private:
  struct MapStorageImpl;

  VkDescriptorPool descriptor_pool{};
  VkDescriptorSetLayout descriptor_set_layout{};
  Scope<MapStorageImpl> pimpl{nullptr};
};

} // namespace Core