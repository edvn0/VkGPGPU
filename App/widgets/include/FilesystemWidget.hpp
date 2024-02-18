#pragma once

#include "Containers.hpp"
#include "Filesystem.hpp"
#include "GenericCache.hpp"
#include "Texture.hpp"
#include "Widget.hpp"

#include <span>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Core;

using TextureCache = GenericCache<Texture, TextureProperties, false>;

class FilesystemWidget : public Widget {
public:
  explicit FilesystemWidget(const Device &, const FS::Path &);

  void on_update(floating ts) override;
  void on_interface(InterfaceSystem &) override;
  void on_create(const Core::Device &, const Core::Window &,
                 const Core::Swapchain &) override;
  void on_destroy() override;
  auto on_notify(const ECS::Message &message) -> void override {}

  auto add_ignored_extensions(const std::span<const std::string> &extensions) {
    for (const auto &ext : extensions) {
      ignored_extensions.insert(ext);
    }
  }

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
  std::jthread pop_one_from_texture_cache_thread;
  std::unordered_set<std::string> ignored_extensions;

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
