#pragma once

#include "Types.hpp"

#include <vulkan/vulkan.h>

namespace Core::UI {

auto add_image(VkSampler sampler, VkImageView image_view, VkImageLayout layout)
    -> VkDescriptorSet;

}
