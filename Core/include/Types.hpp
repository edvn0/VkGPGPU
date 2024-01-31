#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Core {

using usize = std::size_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

#ifdef GPGPU_DOUBLE_PRECISION
using floating = std::double_t;
#else
using floating = std::float_t;
#endif

namespace Detail {
struct DefaultDelete {
  template <class T> auto operator()(T *ptr) const noexcept -> void {
    delete ptr;
  }
};
} // namespace Detail

template <class T, class Deleter = Detail::DefaultDelete>
using Scope = std::unique_ptr<T, Deleter>;
template <class T> using Ref = std::shared_ptr<T>;
template <class T> using Weak = std::weak_ptr<T>;

template <class T, class Deleter = Detail::DefaultDelete, typename... Args>
auto make_scope(Args &&...args) -> Scope<T, Deleter> {
  return Scope<T, Deleter>(new T{std::forward<Args>(args)...});
}

template <class T, typename... Args> auto make_ref(Args &&...args) -> Ref<T> {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace Core
