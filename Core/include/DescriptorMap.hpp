#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "Image.hpp"
#include "Texture.hpp"
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

  /***
   * @brief Add a buffer to the descriptor map. Adds to descriptor set 0.
   * @param binding The binding point in the shader
   * @param info The buffer to add
   */
  auto add_for_frames(u32 binding, const Buffer &info) -> void;

  /***
   * @brief Add an image to the descriptor map. Adds to descriptor set 1.
   * @param binding The binding point in the shader
   * @param image The image to add
   */
  auto add_for_frames(u32 binding, const Image &image) -> void;

  /**
   * @brief Add a texture to the descriptor map. Adds to descriptor set 1.
   * @param binding The binding point in the shader
   * @param texture The texture to add
   */
  auto add_for_frames(u32 binding, const Texture &texture) -> void;

  auto bind(const CommandBuffer &buffer, u32 frame,
            VkPipelineLayout binding) const -> void;

  [[nodiscard]] auto get_descriptor_pool() -> VkDescriptorPool;
  [[nodiscard]] auto get_descriptor_set_layout(std::size_t set = 0) const
      -> VkDescriptorSetLayout {
    return descriptor_set_layout.at(set);
  }

  [[nodiscard]] auto get_descriptor_set_layouts() const
      -> std::span<const VkDescriptorSetLayout> {
    return std::span{descriptor_set_layout};
  }

  auto descriptors() -> auto & { return descriptor_sets; }
  [[nodiscard]] auto descriptors() const -> const auto & {
    return descriptor_sets;
  }

private:
  const Device &device;

  std::map<u32, std::vector<VkDescriptorSet>> descriptor_sets{};
  VkDescriptorPool descriptor_pool{nullptr};
  std::vector<VkDescriptorSetLayout> descriptor_set_layout{};
};

} // namespace Core
