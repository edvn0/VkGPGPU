#include "FilesystemWidget.hpp"

#include "UI.hpp"

#include <imgui.h>
#include <imgui_internal.h>

FilesystemWidget::FilesystemWidget(const Device &dev,
                                   const Core::FS::Path &start_path)
    : device(&dev), current_path(start_path), home_path(start_path),
      texture_cache(dev, Texture::construct_shader(
                             dev, {
                                      .format = ImageFormat::UNORM_RGBA8,
                                      .path = FS::icon("loading.png"),
                                      .mip_generation = MipGeneration(1),
                                  })) {
  history.push_back(current_path);

  pop_one_from_texture_cache_thread = std::jthread{
      [this](const auto &stop_token) {
        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(160));
          texture_cache.update_one();
        }
      },
  };
}

void FilesystemWidget::on_create(const Core::Device &, const Core::Window &,
                                 const Core::Swapchain &) {
  load_icons();
}

void FilesystemWidget::on_update(Core::floating ts) {
  // Handle any updates here, if necessary
}

void FilesystemWidget::on_interface(Core::InterfaceSystem &interface_system) {
  if (UI::begin("Filesystem")) {
    render_navigation_buttons();
    render_directory_contents();
    UI::end();
  }
}

void FilesystemWidget::update_directory_cache(const Core::FS::Path &path) {
  std::vector<Core::FS::DirectoryEntry> &entries =
      directory_cache[path.string()];
  entries.clear();

  for (const auto &entry : Core::FS::DirectoryIterator(path)) {
    if (!ignored_extensions.contains(entry.path().extension().string())) {
      // If not ignored, add it to the cache
      entries.push_back(entry);
    }
  }
}

auto FilesystemWidget::get_cached_directory_contents(const Core::FS::Path &path)
    -> const std::vector<Core::FS::DirectoryEntry> & {
  if (!directory_cache.contains(path.string())) {
    update_directory_cache(path);
  }
  return directory_cache.at(path.string());
}

void FilesystemWidget::on_destroy() {
  // Clean up resources, if needed
}

void FilesystemWidget::change_directory(const Core::FS::Path &new_path) {
  if (!current_path.empty()) {
    back_stack.push(current_path);
  }
  current_path = new_path;
  while (!forward_stack.empty()) {
    forward_stack.pop();
  }
}

void FilesystemWidget::render_navigation_buttons() {
  if (back_icon && Core::UI::image_button(*back_icon) && !back_stack.empty()) {
    forward_stack.push(current_path);
    current_path = back_stack.top();
    back_stack.pop();
  }
  ImGui::SameLine();

  if (forward_icon && Core::UI::image_button(*forward_icon) &&
      !forward_stack.empty()) {
    back_stack.push(current_path);
    current_path = forward_stack.top();
    forward_stack.pop();
  }
  ImGui::SameLine();

  if (home_icon && Core::UI::image_button(*home_icon)) {
    change_directory(home_path);
  }
}

void FilesystemWidget::render_directory_contents() {
  auto render_file_or_directory = [&](const auto &entry, const auto size) {
    Extent<u32> extent{
        static_cast<u32>(size),
        static_cast<u32>(size),
    };
    if (entry.is_directory()) {
      if (UI::set_drag_drop_payload(UI::Identifiers::fs_widget_identifier,
                                    entry.path()) &&
          UI::image_button(*directory_icon, {extent})) {
        change_directory(entry);
      }
    } else {
      if (UI::set_drag_drop_payload(UI::Identifiers::fs_widget_identifier,
                                    entry.path())) {
        UI::image(*file_icon, {extent});
      }
    }
  };

  static constexpr auto is_image = [](const auto &path) {
    const auto extension = path.extension().string();
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".bmp" || extension == ".tga" || extension == ".gif" ||
           extension == ".psd" || extension == ".hdr" || extension == ".pic";
  };

  static constexpr float padding = 16.0F;
  static constexpr float thumbnail_size = 64.0F;
  static constexpr auto cell_size = thumbnail_size + padding;
  static constexpr Extent<u32> extent{
      static_cast<u32>(thumbnail_size),
      static_cast<u32>(thumbnail_size),
  };

  float panel_width = ImGui::GetContentRegionAvail().x;
  auto column_count = static_cast<int>(panel_width / cell_size);
  if (column_count < 1) {
    column_count = 1;
  }

  if (ImGui::BeginTable("##DirectoryContent", column_count)) {
    for (const auto &entries = get_cached_directory_contents(current_path);
         const auto &directory_entry : entries) {

      const auto &path = directory_entry.path();
      const auto filename = path.filename();
      std::string filename_string = filename.string();
      ImGui::PushID(path.c_str());

      if (is_image(path)) {
        const auto &texture = texture_cache.put_or_get(TextureProperties{
            .format = ImageFormat::UNORM_RGBA8,
            .identifier = filename_string,
            .path = path,
            .extent = extent,
            .tiling = ImageTiling::Linear,
            .usage = ImageUsage::ColourAttachment | ImageUsage::Sampled |
                     ImageUsage::TransferSrc | ImageUsage::TransferDst,
            .layout = ImageLayout::ShaderReadOnlyOptimal,
            .mip_generation = MipGeneration(1),
            .resize = ResizeStrategy{extent},
        });

        UI::image_button(*texture, {extent});
      } else {
        render_file_or_directory(directory_entry, thumbnail_size);
      }

      [[maybe_unused]] auto could = UI::set_drag_drop_payload(
          UI::Identifiers::fs_widget_identifier, path);

      UI::text_wrapped("{}", filename_string);

      ImGui::TableNextColumn();

      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

void FilesystemWidget::load_icons() {
  back_icon =
      Texture::construct_shader(*device, {.format = ImageFormat::UNORM_RGBA8,
                                          .path = FS::icon("back.png"),
                                          .mip_generation = MipGeneration(1)});
  forward_icon =
      Texture::construct_shader(*device, {.format = ImageFormat::UNORM_RGBA8,
                                          .path = FS::icon("forward.png"),
                                          .mip_generation = MipGeneration(1)

                                         });
  home_icon =
      Texture::construct_shader(*device, {.format = ImageFormat::UNORM_RGBA8,
                                          .path = FS::icon("home.png"),
                                          .mip_generation = MipGeneration(1)

                                         });
  file_icon =
      Texture::construct_shader(*device, {.format = ImageFormat::UNORM_RGBA8,
                                          .path = FS::icon("file.png"),
                                          .mip_generation = MipGeneration(1)});
  directory_icon =
      Texture::construct_shader(*device, {
                                             .format = ImageFormat::UNORM_RGBA8,
                                             .path = FS::icon("directory.png"),
                                             .mip_generation = MipGeneration(1),
                                         });
}
