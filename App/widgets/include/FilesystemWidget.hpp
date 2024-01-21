#pragma once

#include "Filesystem.hpp"
#include "Texture.hpp"
#include "Widget.hpp"

#include <string>
#include <vector>

using namespace Core;

class FilesystemWidget : public Widget {
public:
  explicit FilesystemWidget(const Device &, const FS::Path &);

  void on_update(floating ts) override;
  void on_interface(InterfaceSystem &) override;
  void on_create() override;
  void on_destroy() override;

private:
  const Device *device;

  FS::Path current_path;
  const FS::Path home_path;
  floating column_width{100.0F};
  std::vector<FS::Path> history;
  int history_index = 0;

  Scope<Texture> back_icon;
  Scope<Texture> forward_icon;
  Scope<Texture> home_icon;
  Scope<Texture> file_icon;
  Scope<Texture> directory_icon;

  std::unordered_map<std::string, std::vector<Core::FS::DirectoryEntry>>
      directory_cache;

  auto update_directory_cache(const Core::FS::Path &path) -> void;
  auto get_cached_directory_contents(const Core::FS::Path &path)
      -> const std::vector<Core::FS::DirectoryEntry> &;

  void change_directory(const FS::Path &new_path);
  void render_navigation_buttons();
  void render_directory_contents();

  void load_icons(); // Function to load icons
};
