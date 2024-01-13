#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "ImageProperties.hpp"

namespace Core {

auto load_image_from_file_to_databuffer(const Core::FS::Path &path)
    -> DataBuffer;

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

class Image {
  struct ImageStorageImpl;

public:
  Image(const Device &device, ImageProperties properties);

  template <std::floating_point T>
  Image(const Device &device, Extent<T> extent, ImageFormat format)
      : Device(device, extent.template as<u32>(), format) {}

  ~Image();

  auto get_descriptor_info() const -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_vulkan_type() const noexcept -> VkDescriptorType;

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
