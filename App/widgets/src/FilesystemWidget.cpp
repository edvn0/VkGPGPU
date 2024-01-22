#include "FilesystemWidget.hpp"

#include "UI.hpp"

#include <imgui.h>
#include <imgui_internal.h>

FilesystemWidget::FilesystemWidget(const Device &dev,
                                   const Core::FS::Path &start_path)
    : device(&dev), current_path(start_path), home_path(start_path) {
  history.push_back(current_path);
}

void FilesystemWidget::on_create() { load_icons(); }

void FilesystemWidget::on_update(Core::floating ts) {
  // Handle any updates here, if necessary
}

void FilesystemWidget::on_interface(Core::InterfaceSystem &interface_system) {
  UI::begin("Filesystem");
  render_navigation_buttons();
  render_directory_contents();
  UI::end();
}

void FilesystemWidget::update_directory_cache(const Core::FS::Path &path) {
  std::vector<Core::FS::DirectoryEntry> &entries =
      directory_cache[path.string()];
  entries.clear();

  for (const auto &entry : Core::FS::DirectoryIterator(path)) {
    entries.push_back(entry);
  }
}

const std::vector<Core::FS::DirectoryEntry> &
FilesystemWidget::get_cached_directory_contents(const Core::FS::Path &path) {
  if (!directory_cache.contains(path.string())) {
    update_directory_cache(path);
  }
  return directory_cache.at(path.string());
}

void FilesystemWidget::on_destroy() {
  // Clean up resources, if needed
}

void FilesystemWidget::change_directory(const Core::FS::Path &new_path) {
  current_path = new_path;
  history.push_back(current_path);
  history_index++;
}

void FilesystemWidget::render_navigation_buttons() {
  // Back Button
  if (back_icon && Core::UI::image_button(*back_icon) && history_index > 0) {
    current_path = history[--history_index];
  }
  ImGui::SameLine();

  // Forward Button
  if (forward_icon && Core::UI::image_button(*forward_icon) &&
      history_index < history.size() - 1) {
    current_path = history[++history_index];
  }
  ImGui::SameLine();

  // Home Button
  if (home_icon && Core::UI::image_button(*home_icon)) {
    change_directory(home_path); // Assuming 'home_path' is defined
  }

  // Slider for adjusting column width, placed in the top right
  ImGui::SameLine(ImGui::GetWindowWidth() -
                  200.0f);      // Adjust for slider width and padding
  ImGui::PushItemWidth(150.0f); // Width of the slider
  ImGui::SliderFloat("##ColumnWidth", &column_width, 50.0f, 300.0f,
                     "Column Width: %.0f");
  ImGui::PopItemWidth();
}

void FilesystemWidget::render_directory_contents() {
  const int columns = 10; // Number of columns in the grid

  auto render_file_or_directory = [&](const auto &entry) {
    if (entry.is_directory()) {
      if (UI::image_button(*directory_icon)) {
        change_directory(entry.path());
      }
      ImGui::NewLine();
      ImGui::Text("%s", entry.path().filename().string().c_str());
    } else {
      UI::image(*file_icon);
      ImGui::NewLine();
      ImGui::Text("%s", entry.path().filename().string().c_str());
    }
  };

  if (ImGui::BeginTable("##fileSystemTable", columns)) {
    ImGui::TableHeadersRow();

    int current_column = 0;

    for (const auto &entries = get_cached_directory_contents(current_path);
         const auto &entry : entries) {
      if (current_column >= columns) {
        ImGui::TableNextRow();
        current_column = 0;
      }
      ImGui::TableSetColumnIndex(current_column);
      render_file_or_directory(entry);
      current_column++;
    }

    ImGui::EndTable();
  }
}

void FilesystemWidget::load_icons() {
  back_icon = Texture::construct(*device, FS::icon("back.png"));
  forward_icon = Texture::construct(*device, FS::icon("forward.png"));
  home_icon = Texture::construct(*device, FS::icon("home.png"));
  file_icon = Texture::construct(*device, FS::icon("file.png"));
  directory_icon = Texture::construct(*device, FS::icon("directory.png"));
}
