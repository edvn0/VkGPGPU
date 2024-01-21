#include "UI.hpp"

#include <backends/imgui_impl_vulkan.h>

namespace Core::UI {

auto add_image(VkSampler sampler, VkImageView image_view, VkImageLayout layout)
    -> VkDescriptorSet {
  auto set = ImGui_ImplVulkan_AddTexture(sampler, image_view, layout);
  return set;
}

} // namespace Core::UI
