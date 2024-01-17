#pragma once

#include <algorithm>
#include <optional>
#include <ranges>

namespace Core::Container {

template <typename T>
concept Container = requires(T t) {
  typename T::value_type;
  typename T::size_type;
  typename T::iterator;
  typename T::const_iterator;
  typename T::reference;
  typename T::const_reference;
  { t.begin() } -> std::convertible_to<typename T::iterator>;
  { t.end() } -> std::convertible_to<typename T::iterator>;
  { t.cbegin() } -> std::convertible_to<typename T::const_iterator>;
  { t.cend() } -> std::convertible_to<typename T::const_iterator>;
  { t.size() } -> std::convertible_to<typename T::size_type>;
  { t.empty() } -> std::convertible_to<bool>;
};

auto sort(Container auto &container) -> void {
  std::ranges::sort(container.begin(), container.end());
}

auto sort(Container auto &container, auto &&predicate) -> void {
  std::ranges::sort(container.begin(), container.end(), predicate);
}

} // namespace Core::Container
