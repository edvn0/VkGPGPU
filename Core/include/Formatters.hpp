#pragma once

#include <exception>
#include <fmt/core.h>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

template <>
struct fmt::formatter<std::exception> : fmt::formatter<const char *> {
  auto format(const std::exception &e, auto &ctx) {
    return formatter<const char *>::format(e.what(), ctx);
  }
};
