#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"

namespace Core {

class Texture {
public:
  Texture(const Device &, const FS::Path &path);
  Texture(const Device &, usize, const Extent<u32> &);
  ~Texture();

  [[nodiscard]] auto get_image_info() const noexcept
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_image() const noexcept -> const Image &;
  [[nodiscard]] auto valid() const noexcept -> bool;

  auto write_to_file(const FS::Path &) -> bool;
  [[nodiscard]] auto size_bytes() const -> usize { return data_buffer.size(); }
  [[nodiscard]] auto get_extent() const -> const auto & { return extent; }

  [[nodiscard]] auto hash() const -> usize;

  static auto empty_with_size(const Device &, usize, const Extent<u32> &)
      -> Scope<Texture>;

private:
  const Device *device{nullptr};
  Extent<u32> extent{};
  DataBuffer data_buffer;

  std::string texture_filename{};
  std::unique_ptr<Image> image{nullptr};
};

} // namespace Core
