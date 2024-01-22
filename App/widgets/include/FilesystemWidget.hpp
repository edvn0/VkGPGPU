#pragma once

#include "Containers.hpp"
#include "Filesystem.hpp"
#include "GenericCache.hpp"
#include "Texture.hpp"
#include "Widget.hpp"

#include <stack>
#include <string>
#include <vector>

using namespace Core;

using TextureCache = GenericCache<Texture, TextureProperties>;

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
  i32 history_index{0};

  std::stack<FS::Path> back_stack;
  std::stack<FS::Path> forward_stack;

  Scope<Texture> back_icon;
  Scope<Texture> forward_icon;
  Scope<Texture> home_icon;
  Scope<Texture> file_icon;
  Scope<Texture> directory_icon;

  TextureCache texture_cache;
  Container::StringLikeMap<std::vector<Core::FS::DirectoryEntry>>
      directory_cache;

  auto update_directory_cache(const Core::FS::Path &path) -> void;
  auto get_cached_directory_contents(const Core::FS::Path &path)
      -> const std::vector<Core::FS::DirectoryEntry> &;

  void change_directory(const FS::Path &new_path);
  void render_navigation_buttons();
  void render_directory_contents();

  void load_icons(); // Function to load icons
};
