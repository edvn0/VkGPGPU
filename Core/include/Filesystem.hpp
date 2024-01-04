#pragma once

#include <concepts>
#include <filesystem>
#include <string>

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

} // namespace Core::FS