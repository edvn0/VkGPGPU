#pragma once

#include "Exception.hpp"
#include "Filesystem.hpp"
#include "Types.hpp"

#include <algorithm>
#include <bit>

namespace Core {

class WriteRangeException : public BaseException {
public:
  using BaseException::BaseException;
};

class DataBuffer {
public:
  explicit DataBuffer(std::integral auto input_size)
      : size(static_cast<usize>(input_size)) {
    // Zero is valid here!
  }

  template <typename T>
  auto write(const T *input_data, std::integral auto input_size) -> void {
    if (input_size > size) {
      throw BaseException{"DataBuffer::write: input_size > size"};
    }
    // Do i need to cast the input_data to u8*?
    const auto input_data_as_u8 = std::bit_cast<const u8 *>(input_data);
    std::copy(input_data_as_u8, input_data_as_u8 + input_size, data.get());
  }

  [[nodiscard]] auto valid() const noexcept -> bool {
    return data != nullptr && size > 0;
  }
  [[nodiscard]] operator bool() const noexcept { return valid(); }

  static auto empty() { return DataBuffer{0}; }

private:
  usize size{0};
  std::unique_ptr<u8[]> data{nullptr};

  // If i want to store any data here, what type should I use?
};

} // namespace Core
