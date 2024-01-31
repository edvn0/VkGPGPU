#pragma once

#include "Types.hpp"

#include <algorithm>
#include <random>
#include <ranges>
#include <sstream>

namespace ECS::UUID {

namespace Detail {
static std::random_device rd;

static std::mt19937_64 e2(rd());

static std::uniform_int_distribution<Core::u64>
    dist(std::llround(std::pow(2, 61)), std::llround(std::pow(2, 62)));

} // namespace Detail

template <Core::usize Bytes>
  requires(Bytes == 32 || Bytes == 64 || Bytes == 128 || Bytes == 256)
inline auto generate_uuid() -> Core::u64 {
  auto uuid = Detail::dist(Detail::e2);
  if constexpr (Bytes == 32) {
    return uuid & 0xFFFFFFFF;
  } else if constexpr (Bytes == 64) {
    return uuid;
  } else if constexpr (Bytes == 128) {
    return uuid;
  } else if constexpr (Bytes == 256) {
    return uuid;
  }
  return 0;
}

} // namespace ECS::UUID
