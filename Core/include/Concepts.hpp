#pragma once

#include <string>
#include <string_view>
#include <type_traits>

namespace Core {

template <class T>
concept IsBuiltin = std::is_fundamental_v<T>;

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
    std::is_same_v<const char *, T> || std::is_same_v<std::string_view, T> ||
    std::is_same_v<std::string, T> || CStrConvertibleTo<T, const char *> ||
    CStrConvertibleTo<T, const wchar_t *>;

} // namespace Core
