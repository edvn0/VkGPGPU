#pragma once

#include "ImageProperties.hpp"

namespace Core {

template <class T> struct IResizeDependent {
  virtual ~IResizeDependent() = default;
  virtual auto resize(const T &) -> void = 0;
};

} // namespace Core
