#pragma once

#include "Exception.hpp"
#include "Filesystem.hpp"
#include "Types.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fmt/format.h>
#include <memory>

namespace Core {

class WriteRangeException final : public BaseException {
public:
  using BaseException::BaseException;
};

class DataBuffer {
public:
  explicit DataBuffer(std::integral auto input_size)
      : buffer_size(static_cast<usize>(input_size)) {
    // Zero is valid here!
  }

  template <typename T>
  auto write(const T *input_data, std::integral auto input_size) -> void {
    if (input_size > buffer_size) {
      throw WriteRangeException{"DataBuffer::write: input_size > size"};
    }
    if (!data) {
      allocate_storage(input_size);
    }
    // Do i need to cast the input_data to u8*?
    std::memcpy(data.get(), input_data, input_size);
  }

  template <typename T, std::size_t Extent = std::dynamic_extent>
  auto read(std::span<T, Extent> output, std::integral auto input_size)
      -> void {
    if (input_size > buffer_size) {
      throw WriteRangeException{"DataBuffer::read: input_size > size"};
    }
    if (!data) {
      throw WriteRangeException{"DataBuffer::read: data is null"};
    }
    // Do i need to cast the input_data to u8*?
    std::memcpy(output.data(), data.get(), input_size);
  }

  /***
   * @brief Read a vector of T from the databuffer
   * @tparam T The type of the vector
   * @param output The vector to write to
   * @param input_count The number of elements to read
   */
  template <typename T>
  auto read(std::vector<T> &output, std::integral auto input_count) -> void {
    const auto actual_size = input_count * sizeof(T);
    // check vector size too
    if (output.size() < input_count) {
      throw WriteRangeException{"DataBuffer::read: input_count > vector size"};
    }
    if (actual_size > buffer_size) {
      throw WriteRangeException{"DataBuffer::read: input_count > size"};
    }
    if (!data) {
      throw WriteRangeException{"DataBuffer::read: data is null"};
    }
    // Do i need to cast the input_data to u8*?
    std::memcpy(output.data(), data.get(), actual_size);
  }

  /***
   * @brief Read an array of T from the databuffer
   * @tparam T The type of the vector
   * @tparam Count The number of elements to read
   * @param output The vector to write to
   */
  template <typename T, std::size_t Count>
  auto read(std::array<T, Count> &output) -> void {
    if (sizeof(T) * Count > buffer_size) {
      throw WriteRangeException{"DataBuffer::read: input_size > size"};
    }
    if (!data) {
      throw WriteRangeException{"DataBuffer::read: data is null"};
    }
    // Do i need to cast the input_data to u8*?
    std::memcpy(output.data(), data.get(), sizeof(T) * Count);
  }

  auto copy_from(const DataBuffer &from) {
    buffer_size = from.size();
    std::memcpy(data.get(), from.data.get(), from.size());
  }

  [[nodiscard]] auto size() const noexcept -> usize { return buffer_size; }
  [[nodiscard]] auto valid() const noexcept -> bool {
    return data != nullptr && buffer_size > 0;
  }
  [[nodiscard]] operator bool() const noexcept { return valid(); }

  static auto empty() { return DataBuffer{0}; }
  static auto copy(const DataBuffer &from) -> DataBuffer {
    DataBuffer constructed(from.size());
    constructed.allocate_storage(constructed.size());
    constructed.copy_from(from);
    return constructed;
  }

private:
  usize buffer_size{0};
  std::unique_ptr<u8[]> data{nullptr};

  auto allocate_storage(std::integral auto new_size) {
    if (data) {
      info("Resetting data storage at {}", fmt::ptr(data.get()));
      data.reset();
    }
    data = std::make_unique<u8[]>(new_size);
  }
};

} // namespace Core
