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

template <class T, class To>
concept CStrConvertibleTo = std::convertible_to<T, To> || requires(const T &t) {
  { t.c_str() } -> std::convertible_to<To>;
};

template <class T>
concept StringLike =
    CStrConvertibleTo<T, const char *> || CStrConvertibleTo<T, const wchar_t *>;

auto shader(StringLike auto path, bool resolve = false)
    -> std::filesystem::path {
  const auto output = std::filesystem::path("shaders") / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

auto pipeline_cache(StringLike auto path, bool resolve = false)
    -> std::filesystem::path {
  const auto output = std::filesystem::path("pipeline_cache") / path;
  if (resolve) {
    return std::filesystem::absolute(output);
  } else {
    return output;
  }
}

auto mkdir_safe(StringLike auto path) -> bool {
  const auto resolved = std::filesystem::absolute(path);
  if (std::filesystem::exists(resolved)) {
    info("Path {} already exists.", resolved);
    return false;
  }

  if (const auto current = std::filesystem::current_path();
      resolved.parent_path() != current) {
    info("Path {} does not share the same parent as {}.", resolved, current);
    return false;
  }

  return std::filesystem::create_directory(resolved);
}

} // namespace Core::FS