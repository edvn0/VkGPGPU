#include "pch/vkgpgpu_pch.hpp"

#include "Formatters.hpp"

#include "Buffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "ImageProperties.hpp"

#include <fmt/format.h>
#include <vulkan/vulkan_core.h>

auto fmt::formatter<Core::Buffer::Type, char, void>::format(
    const Core::Buffer::Type &type, format_context &ctx) const
    -> decltype(ctx.out()) {

  std::string output_type = "Unknown";
  switch (type) {
  case Core::Buffer::Type::Vertex:
    output_type = "Vertex";
    break;
  case Core::Buffer::Type::Index:
    output_type = "Index";
    break;

  case Core::Buffer::Type::Uniform:
    output_type = "Uniform";
    break;
  case Core::Buffer::Type::Storage:
    output_type = "Storage";
    break;
  default:
    break;
  }
  return formatter<const char *>::format(fmt::format("{}", output_type).data(),
                                         ctx);
}

auto fmt::formatter<Core::Queue::Type>::format(const Core::Queue::Type &type,
                                               format_context &ctx) const
    -> decltype(ctx.out()) {
  std::string output_type = "Unknown";
  switch (type) {
  case Core::Queue::Type::Graphics:
    output_type = "Graphics";
    break;
  case Core::Queue::Type::Compute:
    output_type = "Compute";
    break;
  case Core::Queue::Type::Transfer:
    output_type = "Transfer";
    break;
  case Core::Queue::Type::Present:
    output_type = "Present";
    break;
  default:
    break;
  }
  return formatter<const char *>::format(fmt::format("{}", output_type).data(),
                                         ctx);
}

auto fmt::formatter<std::filesystem::path>::format(
    const std::filesystem::path &type, format_context &ctx) const
    -> decltype(ctx.out()) {
  return formatter<const char *>::format(
      fmt::format("{}", type.string()).data(), ctx);
}

auto fmt::formatter<VkDescriptorSet>::format(const VkDescriptorSet &type,
                                             format_context &ctx) const
    -> decltype(ctx.out()) {
  return formatter<const char *>::format(
      fmt::format("set={}", fmt::ptr((const void *)type)).data(), ctx);
}

auto fmt::formatter<Core::Extent<Core::u32>>::format(
    const Core::Extent<Core::u32> &extent, format_context &ctx) const
    -> decltype(ctx.out()) {
  auto formatted = fmt::format("extent={},{}", extent.width, extent.height);
  return formatter<const char *>::format(formatted.data(), ctx);
}

auto fmt::formatter<VkResult>::format(const VkResult &result,
                                      format_context &ctx) const
    -> decltype(ctx.out()) {
  auto formatted = fmt::format("Result:{}", static_cast<Core::i32>(result));
  return formatter<const char *>::format(formatted.data(), ctx);
}
