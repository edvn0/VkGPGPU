#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "ImageProperties.hpp"

namespace Core {

auto load_databuffer_from_file(const FS::Path &path) -> DataBuffer;
auto load_databuffer_from_file(const FS::Path &path, Extent<u32> &)
    -> DataBuffer;
auto load_databuffer_from_file(FS::StringLike auto path) -> DataBuffer {
  return load_databuffer_from_file(path);
}
auto load_databuffer_from_file(FS::StringLike auto path, Extent<u32> &extent)
    -> DataBuffer {
  return load_databuffer_from_file(path, extent);
}

struct ImageProperties {
  Extent<u32> extent;
  ImageFormat format;
  ImageTiling tiling{ImageTiling::Optimal};
  ImageUsage usage{ImageUsage::Sampled};
  ImageLayout layout{ImageLayout::Undefined};
  SamplerFilter min_filter{SamplerFilter::Nearest};
  SamplerFilter max_filter{SamplerFilter::Nearest};
  SamplerAddressMode address_mode{SamplerAddressMode::Repeat};
  SamplerBorderColor border_color{SamplerBorderColor::FloatOpaqueBlack};
};

struct ImageInfo {
  VkImageView image_view{};
  VkSampler sampler{};
};

class Image {
  struct ImageStorageImpl;

public:
  Image(const Device &, ImageProperties properties);
  Image(const Device &, ImageProperties properties,
        const DataBuffer &data_buffer);
  ~Image();

  [[nodiscard]] auto get_descriptor_info() const
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_vulkan_type() const noexcept -> VkDescriptorType;
  [[nodiscard]] auto get_extent() const noexcept -> const Extent<u32> &;

private:
  const Device *device{nullptr};
  ImageProperties properties;
  VkDescriptorImageInfo descriptor_image_info{};
  Scope<ImageStorageImpl> impl{nullptr};

  auto initialise_vulkan_image() -> void;
  auto initialise_vulkan_descriptor_info() -> void;

public:
  Image(const Image &) = delete;
  Image(Image &&) = delete;
  Image &operator=(const Image &) = delete;
  Image &operator=(Image &&) = delete;
};

} // namespace Core
