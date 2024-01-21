#include "UI.hpp"

#include "InterfaceSystem.hpp"
#include "PlatformUI.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <fmt/format.h>

template <typename... Args> auto make_id(Args &&...data) {
  // Use a fold expression to concatenate the formatted pointer strings
  return fmt::format(
      "ID:{}", fmt::join(std::initializer_list<const void *>{(&data)...}, ","));
}

namespace Core::UI {

struct StaticDevice {
  const Device *device{nullptr};
  std::mutex device_access_mutex;

  auto perform(auto &&func) -> decltype(func(*device)) {
    std::unique_lock lock{device_access_mutex};
    return func(*device);
  }
};

static StaticDevice device;

auto initialise(const Device &dev) -> void { device.device = &dev; }

auto add_image(VkSampler sampler, VkImageView image_view, VkImageLayout layout)
    -> VkDescriptorSet {
  auto pool = InterfaceSystem::get_image_pool();
  auto set = ImGui_ImplVulkan_AddTexture(sampler, image_view, layout, pool);
  return set;
}

auto begin(const std::string_view name) -> bool {
  return ImGui::Begin(name.data());
}

auto end() -> void { return ImGui::End(); }

auto image(const Texture &texture) -> void {
  const auto &[sampler, view, layout] = texture.get_image_info();
  auto set = add_image(sampler, view, layout);
  return ImGui::Image(set, ImVec2(64, 64));
}

auto image_button(const Texture &texture) -> bool {
  const auto &[sampler, view, layout] = texture.get_image_info();
  auto set = add_image(sampler, view, layout);
  auto made = make_id(sampler, view, layout);
  return ImGui::ImageButton(made.c_str(), set, ImVec2(64, 64));
}

auto image_drop_button(Scope<Core::Texture> &texture) -> void {

  if (texture) {
    const auto &[sampler, view, layout] = texture->get_image_info();
    auto set = add_image(sampler, view, layout);
    if (ImGui::ImageButton("ImageDropButtonTexture", set, ImVec2(512, 512))) {
      device.perform(
          [&](auto &device) { info("Texture: {}", fmt::ptr(texture.get())); });
    }
  } else {
    if (ImGui::Button("Drop Image Here")) {
      // Button logic
    }
  }

  // Use the platform-independent payload handling function
  const auto dropped_file_path =
      Platform::accept_drag_drop_payload(Identifiers::texture_identifier);
  if (!dropped_file_path.empty()) {
    // Create and swap textures based on the dropped file path
    info("Path: {}", dropped_file_path);
  }
}

} // namespace Core::UI

namespace Core::UI::Detail {

auto text_impl(const std::string &data) -> void {
  return ImGui::Text(data.data());
}
} // namespace Core::UI::Detail
