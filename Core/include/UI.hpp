#pragma once

#include "Colours.hpp"
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
static constexpr std::string_view fs_widget_identifier =
    "DRAGDROP_IDENTIFIER_TEXTURE";
}

namespace Toast {
enum class Type : u8;
};

namespace Detail {
auto text_impl(std::string_view) -> void;
auto text_wrapped_impl(std::string_view) -> void;
auto set_drag_drop_payload_impl(std::string_view, std::string_view data)
    -> bool;
auto toast(Toast::Type type, u32 duration_ms, std::string_view) -> void;
auto save_file_dialog(std::string_view) -> std::optional<FS::Path>;
} // namespace Detail

auto push_id() -> void;
auto pop_id() -> void;

auto generate_id() -> const char *;

auto add_image(VkSampler sampler, VkImageView image_view, VkImageLayout layout)
    -> VkDescriptorSet;

auto begin(std::string_view) -> bool;
auto end() -> void;

auto window_size() -> Extent<float>;
auto window_position() -> std::tuple<u32, u32>;

auto widget(const std::string_view name, auto &&func) {
  if (UI::begin(name)) {
    if constexpr (std::is_invocable_r_v<void, decltype(func),
                                        const Extent<float> &,
                                        const std::tuple<u32, u32> &>) {
      const auto &current_size = UI::window_size();
      const auto &current_position = UI::window_position();
      func(current_size, current_position);
    } else if constexpr (std::is_invocable_r_v<void, decltype(func),
                                               const Extent<float> &>) {
      const auto &current_size = UI::window_size();
      func(current_size);
    } else {
      func();
    }
    UI::end();
  }
}
namespace Toast {
enum class Type : u8 { None, Success, Warning, Error, Info };
template <class... Args>
auto toast(Toast::Type type, u32 duration_ms, fmt::format_string<Args...> fmt,
           Args &&...args) {
  return Detail::toast(type, duration_ms,
                       fmt::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
auto toast(Toast::Type type, fmt::format_string<Args...> fmt, Args &&...args) {
  return Detail::toast(type, 3000,
                       fmt::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
auto success(u32 duration_ms, fmt::format_string<Args...> fmt, Args &&...args) {
  return Detail::toast(Type::Success, duration_ms,
                       fmt::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
auto error(u32 duration_ms, fmt::format_string<Args...> fmt, Args &&...args) {
  return Detail::toast(Type::Error, duration_ms,
                       fmt::format(fmt, std::forward<Args>(args)...));
}
} // namespace Toast

struct InterfaceImageProperties {
  Extent<u32> extent{64, 64};
  Colours::Colour colour{Colours::white};
};

auto image(const Texture &, InterfaceImageProperties = {}) -> void;
auto image(const Image &, InterfaceImageProperties = {}) -> void;
auto image_button(const Texture &, InterfaceImageProperties = {}) -> bool;
auto image_button(const Image &, InterfaceImageProperties = {}) -> bool;
auto image_drop_button(Scope<Core::Texture> &, InterfaceImageProperties = {})
    -> void;
auto accept_drag_drop_payload(std::string_view) -> std::string;
auto accept_drag_drop_payload() -> std::string;
auto set_drag_drop_payload(std::string_view payload_type,
                           const StringLike auto &data) -> bool {
  return Detail::set_drag_drop_payload_impl(payload_type, data);
}
auto set_drag_drop_payload(std::string_view, const FS::Path &) -> bool;

auto save_file_dialog(const Core::StringLike auto &string_like)
    -> std::optional<FS::Path> {
  if constexpr (std::is_same_v<std::decay_t<decltype(string_like)>,
                               std::filesystem::path>) {
    const auto as_string = string_like.string();
    return Detail::save_file_dialog(as_string);
  } else {
    return Detail::save_file_dialog(string_like);
  }
}

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
