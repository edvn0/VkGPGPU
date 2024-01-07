#pragma once

#include "Types.hpp"
#include "Verify.hpp"

#include <cstring>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Core {

struct BufferDataImpl;

class Buffer {
public:
  enum class Type {
    Vertex,
    Index,
    Uniform,
    Storage,
  };

  explicit Buffer(u64 input_size, Type buffer_type);
  ~Buffer();
  // Make non-copyable
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;
  // Make movable
  Buffer(Buffer &&) noexcept = default;
  auto operator=(Buffer &&) noexcept -> Buffer & = default;

  [[nodiscard]] auto get_type() const noexcept -> Type { return type; }
  [[nodiscard]] auto get_vulkan_type() const noexcept -> VkDescriptorType {
    switch (type) {
    case Type::Uniform:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case Type::Storage:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    default:
      return unreachable_return<VK_DESCRIPTOR_TYPE_MAX_ENUM>();
    }
  }
  [[nodiscard]] auto get_size() const noexcept -> u64 { return size; }

  [[nodiscard]] auto get_buffer() const noexcept -> VkBuffer;

  [[nodiscard]] auto get_descriptor_info() const noexcept
      -> const VkDescriptorBufferInfo & {
    return descriptor_info;
  }

  template <typename T> void write(std::span<T> data) {
    write(data.data(), data.size() * sizeof(T));
  }
  template <typename T> void write(const T &data) { write(&data, sizeof(T)); }

  void write(const void *data, u64 data_size);

  template <typename T> auto read(std::vector<T> &output, size_t offset = 0) {
    auto data_size = output.size() * sizeof(T);
    auto raw_data = read_raw(offset, data_size);
    std::memcpy(output.data(), raw_data.data(), data_size);
  }

private:
  u64 size{};
  Type type;

  VkDescriptorBufferInfo descriptor_info{};

  Scope<BufferDataImpl> buffer_data{};
  void initialise_vulkan_buffer();

  void initialise_vertex_buffer();
  void initialise_index_buffer();
  void initialise_uniform_buffer();
  void initialise_storage_buffer();

  auto read_raw(size_t offset, size_t data_size) -> std::vector<char>;
};

} // namespace Core

template <>
struct fmt::formatter<Core::Buffer::Type> : formatter<const char *> {
  auto format(const Core::Buffer::Type &type, format_context &ctx) const
      -> decltype(ctx.out());
};