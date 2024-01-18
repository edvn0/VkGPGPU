#pragma once

#include <exception>
#include <fmt/core.h>
#include <fmt/std.h>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

template <> struct fmt::formatter<VkDescriptorSet> : formatter<const char *> {
  auto format(const VkDescriptorSet &set, format_context &ctx) const
      -> decltype(ctx.out());
};
