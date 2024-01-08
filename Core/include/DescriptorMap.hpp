#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "Types.hpp"
#include <map>
#include <vulkan/vulkan.h>

template <> struct fmt::formatter<VkDescriptorSet> : formatter<const char *> {
  auto format(const VkDescriptorSet &type, format_context &ctx) const
      -> decltype(ctx.out());
};

namespace Core {

class DescriptorMap {
  using DescriptorSets = std::vector<VkDescriptorSet>;

public:
  explicit DescriptorMap(const Device &);
  ~DescriptorMap();

  auto add_for_frames(u32 set, u32 binding, const Buffer &info) -> void;

  auto bind(const CommandBuffer &buffer, u32 frame,
            VkPipelineLayout binding) const -> void;

  [[nodiscard]] auto get_descriptor_pool() -> VkDescriptorPool;
  [[nodiscard]] auto get_descriptor_set_layout() const
      -> VkDescriptorSetLayout {
    return descriptor_set_layout;
  }

  auto descriptors() -> auto & { return descriptor_sets; }
  [[nodiscard]] auto descriptors() const -> const auto & {
    return descriptor_sets;
  }

private:
  const Device &device;

  std::map<u32, std::vector<VkDescriptorSet>> descriptor_sets{};
  VkDescriptorPool descriptor_pool{nullptr};
  VkDescriptorSetLayout descriptor_set_layout{nullptr};
};

} // namespace Core