#include "Image.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Allocator.hpp"
#include "DataBuffer.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

auto load_image_from_file_to_databuffer(const Core::FS::Path &path)
    -> DataBuffer {
  std::int32_t width{};
  std::int32_t height{};
  std::int32_t channels{};

  constexpr auto STBI_rgb_alpha = 4;
  auto *stbi_from_file_data = stbi_load(path.string().c_str(), &width, &height,
                                        &channels, STBI_rgb_alpha);

  if (stbi_from_file_data == nullptr) {
    return DataBuffer::empty();
  }

  const auto size = width * height * STBI_rgb_alpha;
  DataBuffer buffer{size};
  buffer.write(stbi_from_file_data, size);

  stbi_image_free(stbi_from_file_data);
  return buffer;
}

struct Image::ImageStorageImpl {
  const Device *device;
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
  VkImage image{};
  VkImageView image_view{};
  VkSampler sampler{};

  explicit ImageStorageImpl(const Device *dev) : device(dev) {}

  ~ImageStorageImpl() {
    vkDestroySampler(device->get_device(), sampler, nullptr);
    vkDestroyImageView(device->get_device(), image_view, nullptr);

    info("Destroying image and allocation, and destroyed image view and "
         "sampler");

    Allocator allocator{"Image"};
    allocator.deallocate_image(allocation, image);
  }
};

Image::Image(const Device &dev, ImageProperties props)
    : device(&dev), properties(std::move(props)) {
  ensure(device != nullptr, "Device cannot be null. This class name: {}",
         "Image");
  ensure(properties.extent.width > 0, "Extent width must be greater than 0");
  ensure(properties.extent.height > 0, "Extent height must be greater than 0");
  ensure(properties.format != ImageFormat::Undefined,
         "Format cannot be undefined");
  ensure(properties.layout != ImageLayout::Undefined,
         "Layout cannot be undefined");

  impl = make_scope<Image::ImageStorageImpl>(device);

  initialise_vulkan_image();
  initialise_vulkan_descriptor_info();
}

namespace {
auto to_vulkan_layout(ImageLayout layout) -> VkImageLayout {
  switch (layout) {
  case ImageLayout::Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case ImageLayout::General:
    return VK_IMAGE_LAYOUT_GENERAL;
  case ImageLayout::ColorAttachmentOptimal:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case ImageLayout::DepthStencilAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  case ImageLayout::DepthStencilReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  case ImageLayout::ShaderReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case ImageLayout::TransferSrcOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case ImageLayout::TransferDstOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case ImageLayout::Preinitialized:
    return VK_IMAGE_LAYOUT_PREINITIALIZED;
  case ImageLayout::PresentSrcKHR:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  case ImageLayout::SharedPresentKHR:
    return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
  case ImageLayout::ShadingRateOptimalNV:
    return VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV;
  case ImageLayout::FragmentDensityMapOptimalEXT:
    return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
  case ImageLayout::DepthReadOnlyStencilAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
  case ImageLayout::DepthAttachmentStencilReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
  case ImageLayout::DepthAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  default:
    assert(false);
    return VK_IMAGE_LAYOUT_MAX_ENUM;
  }
}
} // namespace

auto Image::initialise_vulkan_descriptor_info() -> void {
  descriptor_image_info.sampler = impl->sampler;
  descriptor_image_info.imageView = impl->image_view;
  descriptor_image_info.imageLayout = to_vulkan_layout(properties.layout);
}

auto Image::get_descriptor_info() const -> const VkDescriptorImageInfo & {
  return descriptor_image_info;
}

auto Image::get_vulkan_type() const noexcept -> VkDescriptorType {
  static constexpr auto empty_bit = ImageUsage{0};

  const auto has_storage_bit =
      (properties.usage & ImageUsage::Storage) != empty_bit;

  if (has_storage_bit) {
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }

  const auto has_sampled_bit =
      (properties.usage & ImageUsage::Sampled) != empty_bit;
  if (has_sampled_bit) {
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  }

  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

Image::~Image() { impl.reset(); }

auto Image::initialise_vulkan_image() -> void {
  Allocator allocator{"Image"};
  VkImageCreateInfo image_create_info{};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = static_cast<VkFormat>(properties.format);
  image_create_info.extent.width = properties.extent.width;
  image_create_info.extent.height = properties.extent.height;
  image_create_info.extent.depth = 1;
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = static_cast<VkImageTiling>(properties.tiling);
  image_create_info.usage = static_cast<VkImageUsageFlags>(properties.usage);
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  impl->allocation = allocator.allocate_image(
      impl->image, impl->allocation_info, image_create_info,
      {
          .usage = Usage::AUTO_PREFER_DEVICE,
          .creation = Creation::HOST_ACCESS_RANDOM_BIT | Creation::MAPPED_BIT,
      });

  VkImageViewCreateInfo image_view_create_info{};
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.image = impl->image;
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = static_cast<VkFormat>(properties.format);
  image_view_create_info.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = 1;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = 1;

  verify(vkCreateImageView(device->get_device(), &image_view_create_info,
                           nullptr, &impl->image_view),
         "vkCreateImageView", "Failed to create image view");

  VkSamplerCreateInfo sampler_create_info{};
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.magFilter = static_cast<VkFilter>(properties.max_filter);
  sampler_create_info.minFilter = static_cast<VkFilter>(properties.min_filter);
  sampler_create_info.addressModeU =
      static_cast<VkSamplerAddressMode>(properties.address_mode);
  sampler_create_info.addressModeV =

      static_cast<VkSamplerAddressMode>(properties.address_mode);
  sampler_create_info.addressModeW =
      static_cast<VkSamplerAddressMode>(properties.address_mode);
  sampler_create_info.anisotropyEnable = VK_FALSE;
  sampler_create_info.maxAnisotropy = 1.0f;
  sampler_create_info.borderColor =
      static_cast<VkBorderColor>(properties.border_color);
  sampler_create_info.unnormalizedCoordinates = VK_FALSE;
  sampler_create_info.compareEnable = VK_FALSE;
  sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_create_info.mipLodBias = 0.0f;
  sampler_create_info.minLod = 0.0f;
  sampler_create_info.maxLod = 1.0f;

  verify(vkCreateSampler(device->get_device(), &sampler_create_info, nullptr,
                         &impl->sampler),
         "vkCreateSampler", "Failed to create sampler");
}

} // namespace Core
