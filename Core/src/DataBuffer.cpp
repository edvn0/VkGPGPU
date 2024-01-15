#include "DataBuffer.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include <fmt/format.h>

namespace Core {

auto DataBuffer::allocate_storage(usize new_size) -> void {
  if (data) {
    info("Resetting data storage at {}", fmt::ptr(data.get()));
    data.reset();
  }
  data = std::make_unique<u8[]>(new_size);
}

} // namespace Core