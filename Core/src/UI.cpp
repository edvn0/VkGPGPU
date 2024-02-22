#include "UI.hpp"

#include "InterfaceSystem.hpp"
#include "PlatformUI.hpp"
#include "Texture.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <fmt/format.h>

#include "imgui-notify/ImGuiNotify.hpp"

template <typename... Args> auto make_id(Args &&...data) {
  // Use a fold expression to concatenate the formatted pointer strings
  return fmt::format(
      "ID:{}", fmt::join(std::initializer_list<const void *>{(&data)...}, ","));
}

namespace Core::UI {

template <std::integral N>
  requires(!std::is_same_v<N, f32>)
auto to_imvec2(const Extent<N> &extent) -> ImVec2 {
  auto casted = extent.template as<f32>();
  return ImVec2(casted.width, casted.height);
}

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

auto window_size() -> Extent<float> {
  Extent<float> output;

  auto content = ImGui::GetContentRegionAvail();
  output.width = content.x;
  output.height = content.y;
  return output;
}

auto window_position() -> std::tuple<u32, u32> {
  ImVec2 vMin = ImGui::GetWindowContentRegionMin();
  ImVec2 vMax = ImGui::GetWindowContentRegionMax();

  vMin.x += ImGui::GetWindowPos().x;
  vMin.y += ImGui::GetWindowPos().y;
  vMax.x += ImGui::GetWindowPos().x;
  vMax.y += ImGui::GetWindowPos().y;

  ImGui::GetForegroundDrawList()->AddRect(vMin, vMax,
                                          IM_COL32(255, 255, 0, 255));

  return {
      static_cast<u32>(vMin.x),
      static_cast<u32>(vMin.y),
  };
}

auto image(const Texture &texture, InterfaceImageProperties properties)
    -> void {
  const auto &[sampler, view, layout] = texture.get_image_info();
  auto set = add_image(sampler, view, layout);
  auto made = make_id(set, sampler, view, layout, texture.hash());
  ImGui::PushID(made.c_str());
  ImGui::Image(set, to_imvec2(properties.extent));
  ImGui::PopID();
}

auto image(const Image &image, InterfaceImageProperties properties) -> void {
  const auto &[sampler, view, layout] = image.get_descriptor_info();
  auto set = add_image(sampler, view, layout);
  auto made = make_id(set, sampler, view, layout, image.hash());
  ImGui::PushID(made.c_str());
  ImGui::Image(set, to_imvec2(properties.extent));
  ImGui::PopID();
}

auto image_button(const Texture &texture, InterfaceImageProperties properties)
    -> bool {
  const auto &[sampler, view, layout] = texture.get_image_info();
  auto set = add_image(sampler, view, layout);
  auto made = make_id(set, sampler, view, layout, texture.hash());
  return ImGui::ImageButton(made.c_str(), set, to_imvec2(properties.extent));
}

auto image_button(const Image &image, InterfaceImageProperties properties)
    -> bool {
  const auto &[sampler, view, layout] = image.get_descriptor_info();
  auto set = add_image(sampler, view, layout);
  auto made = make_id(set, sampler, view, layout, image.hash());
  return ImGui::ImageButton(made.c_str(), set, to_imvec2(properties.extent));
}

auto image_drop_button(Scope<Core::Texture> &texture,
                       InterfaceImageProperties properties) -> void {
  if (texture) {
    const Texture &textureToShow = *texture;
    if (UI::image_button(textureToShow, properties)) {
      // Button click logic (if any)
    }
  } else {
    if (ImGui::Button("Whatever")) {
      // Button click logic (if any)
    }
  }

  // Use the platform-independent payload handling function
  const auto dropped_file_path =
      Platform::accept_drag_drop_payload(Identifiers::fs_widget_identifier);
  if (!dropped_file_path.empty()) {
    try {
      auto path = std::filesystem::path{dropped_file_path};
      device.perform([&](auto &device) {
        auto new_text = Texture::construct_shader(
            device,
            {
                .format = ImageFormat::UNORM_RGBA8,
                .path = path,
                .usage = ImageUsage::ColourAttachment | ImageUsage::Sampled |
                         ImageUsage::TransferSrc | ImageUsage::TransferDst,
                .mip_generation = MipGeneration(1),
            });
        texture = std::move(new_text);
      });
    } catch (const std::exception &exc) {
      error("Path was :{}, exc:  {}", dropped_file_path, exc.what());
    }
  }
}

auto accept_drag_drop_payload(std::string_view) -> std::string {
  return Platform::accept_drag_drop_payload(Identifiers::fs_widget_identifier);
}

auto accept_drag_drop_payload() -> std::string {
  return Platform::accept_drag_drop_payload(Identifiers::fs_widget_identifier);
}

auto set_drag_drop_payload(const std::string_view payload_identifier,
                           const FS::Path &path) -> bool {
  return Platform::set_drag_drop_payload(payload_identifier, path);
}

} // namespace Core::UI

namespace Core::UI::Detail {

auto text_impl(const std::string_view data) -> void {
  return ImGui::Text("%s", data.data());
}

auto text_wrapped_impl(const std::string_view data) -> void {
  return ImGui::TextWrapped("%s", data.data());
}

auto set_drag_drop_payload_impl(const std::string_view payload_identifier,
                                const std::string_view data) -> bool {
  return Platform::set_drag_drop_payload(payload_identifier, data);
}

constexpr auto to_imgui_notify_type(Toast::Type type) {
  return static_cast<ImGuiToastType>(type);
}

auto toast(Toast::Type type, u32 duration_ms, std::string_view data) -> void {
  ImGuiToast toast{to_imgui_notify_type(type), static_cast<i32>(duration_ms),
                   "%s", data.data()};
  ImGui::InsertNotification(toast);
}

} // namespace Core::UI::Detail
