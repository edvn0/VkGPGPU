#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "ImageProperties.hpp"

namespace Core {
struct ResizeInfo {
  Extent<u32> extent;
  bool resize{false};
};

auto load_databuffer_from_file(const FS::Path &path) -> DataBuffer;
auto load_databuffer_from_file(const FS::Path &path, Extent<u32> &,
                               ResizeInfo = {}) -> DataBuffer;
auto load_databuffer_from_file(StringLike auto path) -> DataBuffer {
  return load_databuffer_from_file(path);
}
auto load_databuffer_from_file(StringLike auto path, Extent<u32> &extent,
                               ResizeInfo info = {}) -> DataBuffer {
  return load_databuffer_from_file(path, extent, info);
}

struct MipInfo {
  // Technically, zero mips is one mip
  u32 mips{1};
  bool use_mips{false};

  constexpr auto valid() const { return use_mips && mips > 1; }
};

struct ImageProperties {
  Extent<u32> extent;

  MipInfo mip_info{};
  ResizeInfo resize_info{};

  bool generate_per_mip_image_views{true};

  ImageFormat format{ImageFormat::Undefined};
  ImageTiling tiling{ImageTiling::Optimal};
  ImageUsage usage{ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc};
  ImageLayout layout{ImageLayout::ColorAttachmentOptimal};
  SamplerFilter min_filter{SamplerFilter::Linear};
  SamplerFilter max_filter{SamplerFilter::Linear};
  SamplerAddressMode address_mode{SamplerAddressMode::Repeat};
  SamplerBorderColor border_color{SamplerBorderColor::FloatOpaqueBlack};
  CompareOperation compare_op{CompareOperation::Less};

  // For threading purposes, lets allow calling code to provide a owning command
  // buffer :)
  CommandBuffer *command_buffer_override{nullptr};
};

class Image {
  struct ImageStorageImpl;

public:
  Image(const Device &, ImageProperties properties);
  Image(const Device &, ImageProperties properties,
        const DataBuffer &data_buffer);
  ~Image();

  auto recreate() -> void;
  auto transition_image_to(ImageLayout) -> void;

  auto get_properties() noexcept -> ImageProperties & { return properties; }
  [[nodiscard]] auto get_properties() const noexcept
      -> const ImageProperties & {
    return properties;
  }
  [[nodiscard]] auto get_descriptor_info() const
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_vulkan_type() const noexcept -> VkDescriptorType;
  [[nodiscard]] auto get_extent() const noexcept -> const Extent<u32> &;
  [[nodiscard]] auto hash() const noexcept -> usize;

  auto get_mip_image_view_at(u32) const -> VkImageView;
  auto get_mip_size(u32) const -> std::pair<u32, u32>;
  auto get_image() const -> VkImage;

  auto get_aspect_bits() const { return aspect_bit; }

  static auto construct_reference(const Device &device,
                                  const ImageProperties &properties)
      -> Ref<Image>;

  auto initialise_per_mip_image_views(u32 override_count = 0) -> void;

private:
  const Device *device{nullptr};
  CommandBuffer *override_command_buffer{nullptr};
  ImageProperties properties;
  VkDescriptorImageInfo descriptor_image_info{};
  Scope<ImageStorageImpl> impl{nullptr};
  ImageLayout current{ImageLayout::Undefined};
  VkImageAspectFlags aspect_bit{VK_IMAGE_ASPECT_COLOR_BIT};

  auto create_mips() -> void;
  auto load_image_data_from_buffer(const DataBuffer &) -> void;
  auto initialise_vulkan_image() -> void;
  auto initialise_vulkan_descriptor_info() -> void;

public:
  Image(const Image &) = delete;
  Image(Image &&) = delete;
  Image &operator=(const Image &) = delete;
  Image &operator=(Image &&) = delete;
};

} // namespace Core
