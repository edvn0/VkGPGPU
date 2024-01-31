#pragma once

#include "Concepts.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <string>

template <>
struct fmt::formatter<std::filesystem::path> : formatter<const char *> {
  auto format(const std::filesystem::path &type, format_context &ctx) const
      -> decltype(ctx.out());
};

namespace Core::FS {

using Path = std::filesystem::path;
using DirectoryEntry = std::filesystem::directory_entry;
using DirectoryIterator = std::filesystem::directory_iterator;
using RecursiveDirectoryIterator =
    std::filesystem::recursive_directory_iterator;

auto resolve(StringLike auto path) -> FS::Path {
  return std::filesystem::absolute(path);
}

inline auto font_directory() { return std::filesystem::path("fonts"); }
auto font(StringLike auto path, bool resolve = true) {
  const auto output = font_directory() / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

inline auto model_directory() { return std::filesystem::path("models"); }
auto model(StringLike auto path, bool resolve = true) {
  const auto output = model_directory() / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

inline auto icon_directory() { return std::filesystem::path("icons"); }
auto icon(StringLike auto path, bool resolve = true) {
  const auto output = icon_directory() / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

inline auto shader_directory() { return std::filesystem::path("shaders"); }
auto shader(StringLike auto path, bool resolve = true)
    -> std::filesystem::path {
  const auto output = shader_directory() / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

inline auto texture_directory() { return std::filesystem::path("textures"); }
auto texture(StringLike auto path, bool resolve = true)
    -> std::filesystem::path {
  const auto output = texture_directory() / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

auto pipeline_cache(StringLike auto path, bool resolve = true)
    -> std::filesystem::path {
  const auto output = std::filesystem::path("pipeline_cache") / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

auto mkdir_safe(StringLike auto path) -> bool {
  const auto resolved = FS::resolve(path);
  if (std::filesystem::exists(resolved)) {
    debug("mkdir_safe Path {} already exists.", resolved);
    return false;
  }

  if (const auto current = std::filesystem::current_path();
      resolved.parent_path() != current) {
    debug("mkdir_safe Path {} does not share the same parent as {}.", resolved,
          current);
    return false;
  }

  return std::filesystem::create_directory(resolved);
}

auto exists(StringLike auto path) -> bool {
  const auto resolved = FS::resolve(path);
  return std::filesystem::exists(resolved);
}

auto set_current_path(StringLike auto path) -> bool {
  const auto resolved = FS::resolve(path);
  if (!std::filesystem::exists(resolved)) {
    debug("set_current_path Path {} does not exist.", resolved);
    return false;
  }

  std::filesystem::current_path(resolved);
  info("set_current_path Path set to {}.", std::filesystem::current_path());
  return true;
}

template <class Func, class Filter, bool Recursive = true>
void for_each_in_directory(const FS::Path &dir, Func process, Filter filter) {
  // First check if the directory exists
  if (!std::filesystem::exists(dir)) {
    debug("for_each_in_directory Path {} does not exist.", dir);
    return;
  }

  // Check if Filter is a lambda or function
  constexpr bool is_lambda =
      std::is_invocable_r<bool, Filter, const FS::DirectoryEntry &>::value;

  // Helper function to apply the filter
  static constexpr auto apply_filter = [](const FS::DirectoryEntry &entry,
                                          const Filter &filter) -> bool {
    if constexpr (is_lambda) {
      // If Filter is a lambda, directly apply it
      return filter(entry);
    } else {
      // Otherwise, assume Filter is a set of extensions and check if the
      // entry's extension is in the set
      return filter.find(entry.path().extension().string()) != filter.end();
    }
  };

  // Determine the iterator type based on the Recursive parameter
  using iterator = std::conditional_t<Recursive, RecursiveDirectoryIterator,
                                      DirectoryIterator>;

  for (const auto &entry : iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue; // Skip if not a regular file
    }

    if (apply_filter(entry, filter)) {
      process(entry); // Apply the user-provided function to the file
    }
  }
}

} // namespace Core::FS
