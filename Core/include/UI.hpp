#pragma once

#include "Texture.hpp"
#include "Types.hpp"

#include <fmt/core.h>
#include <string>
#include <string_view>
#include <vulkan/vulkan.h>

namespace Core::UI {

auto initialise(const Device &) -> void;

namespace Identifiers {
static constexpr std::string_view texture_identifier =
    "DRAGDROP_IDENTIFIER_TEXTURE";
}

namespace Detail {
auto text_impl(const std::string &data) -> void;
}

auto add_image(VkSampler sampler, VkImageView image_view, VkImageLayout layout)
    -> VkDescriptorSet;

auto begin(std::string_view) -> bool;
auto end() -> void;

auto widget(const std::string_view name, auto &&func) {
  if (UI::begin(name)) {
    func();
    UI::end();
  }
}

auto image(const Texture &) -> void;
auto image_button(const Texture &) -> bool;
auto image_drop_button(Scope<Core::Texture> &texture) -> void;

template <typename... Args>
auto text(fmt::format_string<Args...> format, Args &&...args) -> void {
  return Detail::text_impl(fmt::format(format, std::forward<Args>(args)...));
}

} // namespace Core::UI
