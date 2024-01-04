#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Core {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using size = std::size_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

template <class T> using Scope = std::unique_ptr<T>;
template <class T, typename... Args>
auto make_scope(Args &&...args) -> Scope<T> {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

} // namespace Core