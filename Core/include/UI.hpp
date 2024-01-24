#pragma once

#include "Filesystem.hpp"
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
auto text_impl(std::string_view) -> void;
auto text_wrapped_impl(std::string_view) -> void;
auto set_drag_drop_payload_impl(std::string_view, std::string_view data)
    -> bool;
} // namespace Detail

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

namespace Colours {
struct Colour {
  std::array<float, 4> colours;

  constexpr auto r() const -> decltype(auto) { return colours.at(0); };
  constexpr auto g() const -> decltype(auto) { return colours.at(0); };
  constexpr auto b() const -> decltype(auto) { return colours.at(0); };
  constexpr auto a() const -> decltype(auto) { return colours.at(0); };
};

static constexpr auto White = Colour{1.0F, 1.0F, 1.0F, 1.0F};

} // namespace Colours

struct ImageProperties {
  Extent<u32> extent{64, 64};
  Colours::Colour colour{Colours::White};
};

auto image(const Texture &, ImageProperties = {}) -> void;
auto image_button(const Texture &, ImageProperties = {}) -> bool;
auto image_drop_button(Scope<Core::Texture> &, ImageProperties = {}) -> void;
auto accept_drag_drop_payload(std::string_view) -> std::string;
auto set_drag_drop_payload(std::string_view payload_type,
                           const StringLike auto &data) -> bool {
  return Detail::set_drag_drop_payload_impl(payload_type, data);
}
auto set_drag_drop_payload(std::string_view, const FS::Path &) -> bool;

template <typename... Args>
auto text(fmt::format_string<Args...> format, Args &&...args) -> void {
  return Detail::text_impl(fmt::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
auto text_wrapped(fmt::format_string<Args...> format, Args &&...args) -> void {
  return Detail::text_wrapped_impl(
      fmt::format(format, std::forward<Args>(args)...));
}

} // namespace Core::UI
