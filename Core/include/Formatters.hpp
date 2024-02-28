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

template <typename T>
struct fmt::formatter<std::vector<T>> : formatter<const char *> {
  auto format(const std::vector<T> &vector, format_context &ctx) const
      -> decltype(ctx.out()) {
    return formatter<const char *>::format(
        fmt::format("{}", fmt::join(vector, ", ")).data(), ctx);
  }
};
