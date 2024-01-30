#include "pch/vkgpgpu_pch.hpp"

#include "DataBuffer.hpp"

#include <fmt/format.h>

namespace Core {

auto DataBuffer::allocate_storage(usize new_size) -> void {
  if (data) {
    info("Resetting data storage at {}", fmt::ptr(data.get()));
    data.reset();
  }
  data = std::make_unique<u8[]>(new_size);
}

auto human_readable_size(usize bytes, i32 places) -> std::string {
  constexpr std::array<const char *, 5> units = {"Bytes", "KB", "MB", "GB",
                                                 "TB"};
  const auto max_unit_index = units.size() - 1;

  usize unit_index = 0;
  double size = bytes;

  while (size >= 1024 && unit_index < max_unit_index) {
    size /= 1024;
    ++unit_index;
  }

  return fmt::format("{:.{}f} {}", size, places, units[unit_index]);
}

} // namespace Core
