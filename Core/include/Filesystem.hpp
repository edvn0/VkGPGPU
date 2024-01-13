#pragma once

#include "Logger.hpp"

#include <concepts>
#include <filesystem>
#include <string>

template <>
struct fmt::formatter<std::filesystem::path> : formatter<const char *> {
  auto format(const std::filesystem::path &type, format_context &ctx) const
      -> decltype(ctx.out());
};

namespace Core::FS {

using Path = std::filesystem::path;

template <class T, class To>
concept CStrConvertibleTo = std::convertible_to<T, To> || requires(const T &t) {
  { t.c_str() } -> std::convertible_to<To>;
} || requires(const T &t) {
  { t.data() } -> std::convertible_to<To>;
} || requires(const T &t) {
  { t.c_str() } -> std::convertible_to<const char *>;
} || requires(const T &t) {
  { t.data() } -> std::convertible_to<const char *>;
};

template <class T>
concept StringLike =
    CStrConvertibleTo<T, const char *> || CStrConvertibleTo<T, const wchar_t *>;

auto resolve(StringLike auto path) -> std::filesystem::path {
  return std::filesystem::absolute(path);
}

auto shader(StringLike auto path, bool resolve = true)
    -> std::filesystem::path {
  const auto output = std::filesystem::path("shaders") / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

auto texture(StringLike auto path, bool resolve = true)
    -> std::filesystem::path {
  const auto output = std::filesystem::path("textures") / path;
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

} // namespace Core::FS
