#pragma once

#include "Exception.hpp"
#include "Logger.hpp"

#include <cassert> // for assert (or your preferred debug break method)

namespace Core {

template <typename T = void> [[noreturn]] auto unreachable_return() {
  throw BaseException{"Invalidly here!"};
}
template <auto T> [[noreturn]] auto unreachable_return() -> decltype(T) {
  throw BaseException{"Invalidly here!"};
}

template <typename... Args>
void ensure(bool condition, fmt::format_string<Args...> message,
            Args &&...args) {
  if (!condition) {
    auto formatted_message = fmt::format(message, std::forward<Args>(args)...);
    // Log the formatted message, throw an exception, or handle the error as
    // needed For example, using assert to trigger a debug break
    error("{}", formatted_message);
    assert(false);
    unreachable_return();
  }
}

} // namespace Core
