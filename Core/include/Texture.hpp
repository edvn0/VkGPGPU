#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"

namespace Core {

class Texture {
public:
  Texture(const Device &, const FS::Path &path);
  Texture(const Device &, DataBuffer &&);
  ~Texture();

  [[nodiscard]] auto get_image_info() const noexcept
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto valid() const noexcept -> bool;

  auto write_to_file(const FS::Path &) -> bool;

private:
  const Device *device{nullptr};
  DataBuffer data_buffer;
  std::unique_ptr<Image> image{nullptr};
};

} // namespace Core
