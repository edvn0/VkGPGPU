#include "pch/vkgpgpu_pch.hpp"

#include "Image.hpp"

#include "Allocator.hpp"
#include "CommandBuffer.hpp"
#include "DataBuffer.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <cstring>
#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace Core {

auto to_vulkan_format(ImageFormat format) -> VkFormat {
  switch (format) {
    using enum Core::ImageFormat;
  case UNORM_RGBA8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case SRGB_RGBA8:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case SRGB_RGBA32:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case Undefined:
    return VK_FORMAT_UNDEFINED;
  case DEPTH32F:
    return VK_FORMAT_D32_SFLOAT;
  case DEPTH24STENCIL8:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case DEPTH16:
    return VK_FORMAT_D16_UNORM;
  default:
    assert(false);
    return VK_FORMAT_MAX_ENUM;
  }
}

bool transition_image(
    const ImmediateCommandBuffer &buffer, VkImage &to_transition,
    VkImageLayout from, VkImageLayout to, u32 mip_levels = 1,
    VkImageAspectFlags aspect_bit = VK_IMAGE_ASPECT_COLOR_BIT) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = to_transition;
  barrier.subresourceRange.aspectMask = aspect_bit;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mip_levels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
      to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             to == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    ensure(false, "Unsupported layout transition");
  }

  VkDependencyFlags flags = VK_DEPENDENCY_BY_REGION_BIT;
  vkCmdPipelineBarrier(buffer.get_command_buffer(), sourceStage,
                       destinationStage,
                       flags,      // Dependency flags
                       0, nullptr, // Memory barrier count and data
                       0, nullptr, // Buffer memory barrier count and data
                       1, &barrier // Image memory barrier count and data
  );

  return true;
}

namespace {
auto to_vulkan_layout(ImageLayout layout) -> VkImageLayout {
  switch (layout) {
    using enum Core::ImageLayout;
  case Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case General:
    return VK_IMAGE_LAYOUT_GENERAL;
  case ColorAttachmentOptimal:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case DepthStencilAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  case DepthStencilReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  case ShaderReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case TransferSrcOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case TransferDstOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case Preinitialized:
    return VK_IMAGE_LAYOUT_PREINITIALIZED;
  case PresentSrcKHR:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  case SharedPresentKHR:
    return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
  case ShadingRateOptimalNV:
    return VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV;
  case FragmentDensityMapOptimalEXT:
    return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
  case DepthReadOnlyStencilAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
  case DepthAttachmentStencilReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
  case DepthAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  default:
    assert(false);
    return VK_IMAGE_LAYOUT_MAX_ENUM;
  }
}
} // namespace

auto load_databuffer_from_file(const FS::Path &path) -> DataBuffer {
  if (!FS::exists(path)) {
    error("File does not exist: {}", path);
    return DataBuffer::empty();
  }
  std::int32_t width{};
  std::int32_t height{};
  std::int32_t actual_channels{};
  auto *stbi_pixels = stbi_load(path.string().c_str(), &width, &height,
                                &actual_channels, STBI_rgb_alpha);

  DataBuffer databuffer{
      static_cast<std::size_t>(width * height * actual_channels)};
  databuffer.write(stbi_pixels, databuffer.size());

  stbi_image_free(stbi_pixels);
  return databuffer;
}

auto load_databuffer_from_file(const FS::Path &path, Extent<u32> &output_extent)
    -> DataBuffer {
  if (!FS::exists(path)) {
    error("File does not exist: {}", path);
    return DataBuffer::empty();
  }
  std::int32_t width{};
  std::int32_t height{};
  std::int32_t actual_channels{};
  auto *stbi_pixels = stbi_load(path.string().c_str(), &width, &height,
                                &actual_channels, STBI_rgb_alpha);

  DataBuffer databuffer{
      static_cast<std::size_t>(width * height * STBI_rgb_alpha)};
  databuffer.write(stbi_pixels, databuffer.size());

  stbi_image_free(stbi_pixels);

  output_extent = {
      static_cast<u32>(width),
      static_cast<u32>(height),
  };

  return databuffer;
}

struct Image::ImageStorageImpl {
  const Device *device;
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
  VkImage image{};
  VkImageView image_view{};
  VkSampler sampler{};

  std::unordered_map<u32, VkImageView> per_mip_image_views{};

  explicit ImageStorageImpl(const Device *dev) : device(dev) {}

  ~ImageStorageImpl() {
    vkDestroySampler(device->get_device(), sampler, nullptr);
    vkDestroyImageView(device->get_device(), image_view, nullptr);
    Allocator allocator{"Image"};
    allocator.deallocate_image(allocation, image);

    for (auto &&[k, img] : per_mip_image_views) {
      vkDestroyImageView(device->get_device(), img, nullptr);
    }
  }
};

auto to_aspect_bit(const ImageFormat &format) -> VkImageAspectFlags {
  if (format == ImageFormat::DEPTH16 ||
      format == ImageFormat::DEPTH24STENCIL8 || format == ImageFormat::DEPTH32F)
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  return VK_IMAGE_ASPECT_COLOR_BIT;
}

Image::Image(const Device &dev, ImageProperties props)
    : device(&dev), properties(props),
      aspect_bit(to_aspect_bit(properties.format)) {
  ensure(device != nullptr, "Device cannot be null. This class name: {}",
         "Image");
  ensure(properties.extent.width > 0, "Extent width must be greater than 0");
  ensure(properties.extent.height > 0, "Extent height must be greater than 0");
  ensure(properties.format != ImageFormat::Undefined,
         "Format cannot be undefined");
  ensure(properties.layout != ImageLayout::Undefined,
         "Layout cannot be undefined");
  if (aspect_bit == VK_IMAGE_ASPECT_DEPTH_BIT) {
    properties.mip_info.mips = 1;
    properties.mip_info.use_mips = false;
  }

  recreate();
  if (aspect_bit == VK_IMAGE_ASPECT_DEPTH_BIT) {
    transition_image(create_immediate(*device, Queue::Type::Graphics),
                     impl->image, VK_IMAGE_LAYOUT_UNDEFINED,
                     to_vulkan_layout(properties.layout), 1, aspect_bit);
  }
}

auto Image::construct_reference(const Device &device,
                                const ImageProperties &properties)
    -> Ref<Image> {
  return make_ref<Image>(device, properties);
}

auto Image::recreate() -> void {
  impl.reset(new ImageStorageImpl(device));

  initialise_vulkan_image();
  initialise_vulkan_descriptor_info();
  if (properties.generate_per_mip_image_views) {
    initialise_per_mip_image_views();
  }

  if (properties.mip_info.valid()) {
    create_mips();
  }
}

auto Image::load_image_data_from_buffer(const DataBuffer &data_buffer) const
    -> void {
  // Create a transfer buffer
  Allocator allocator{"Image"};
  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = data_buffer.size();
  buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VkBuffer staging_buffer{};
  VmaAllocationInfo staging_allocation_info{};
  const auto allocation = allocator.allocate_buffer(
      staging_buffer, staging_allocation_info, buffer_create_info,
      {
          .usage = Usage::AUTO_PREFER_DEVICE,
          .creation = Creation::HOST_ACCESS_RANDOM_BIT | Creation::MAPPED_BIT,
      });
  const std::span allocation_span{
      static_cast<u8 *>(staging_allocation_info.pMappedData),
      staging_allocation_info.size};
  data_buffer.read(allocation_span);

  {
    const auto buffer = create_immediate(*device, Queue::Type::Graphics);

    transition_image(buffer, impl->image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspect_bit);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = aspect_bit;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {
        0,
        0,
        0,
    };
    region.imageExtent = {
        properties.extent.width,
        properties.extent.height,
        1,
    };

    vkCmdCopyBufferToImage(buffer.get_command_buffer(), staging_buffer,
                           impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    transition_image(buffer, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     to_vulkan_layout(properties.layout), aspect_bit);
  }

  allocator.deallocate_buffer(allocation, staging_buffer);
}

Image::Image(const Device &dev, ImageProperties properties,
             const DataBuffer &data_buffer)
    : Image(dev, properties) {
  load_image_data_from_buffer(data_buffer);
  create_mips();
}

auto Image::initialise_vulkan_descriptor_info() -> void {
  descriptor_image_info.sampler = impl->sampler;
  descriptor_image_info.imageView = impl->image_view;
  descriptor_image_info.imageLayout = to_vulkan_layout(properties.layout);
}

auto Image::initialise_per_mip_image_views() -> void {
  for (auto mip = 0U; mip < properties.mip_info.mips; mip++) {
    VkImageAspectFlags aspectMask = aspect_bit;
    if (properties.format == ImageFormat::DEPTH24STENCIL8)
      aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkFormat vulkanFormat = to_vulkan_format(properties.format);

    VkImageViewCreateInfo image_view_create_info = {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = vulkanFormat;
    image_view_create_info.flags = 0;
    image_view_create_info.subresourceRange = {};
    image_view_create_info.subresourceRange.aspectMask = aspectMask;
    image_view_create_info.subresourceRange.baseMipLevel = mip;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;
    image_view_create_info.image = impl->image;

    verify(vkCreateImageView(device->get_device(), &image_view_create_info,
                             nullptr, &impl->per_mip_image_views[mip]),
           "vkCreateImageView", "failed to create");
  }
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

auto Image::get_extent() const noexcept -> const Extent<u32> & {
  return properties.extent;
}

auto Image::get_mip_image_view_at(u32 mip) const -> VkImageView {
  return impl->per_mip_image_views.at(mip);
}

auto Image::get_mip_size(u32 mip) const -> std::pair<u32, u32> {
  Extent<u32> final_extent{properties.extent};
  while (mip > 0) {
    final_extent /= 2;
    mip /= 2;
  }
  return final_extent.as_pair();
}

auto Image::get_image() const -> VkImage { return impl->image; }

namespace {
auto hash_combine(usize &seed, const void *value) noexcept -> void {
  static constexpr auto hasher = std::hash<const void *>{};
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} // namespace

auto Image::hash() const noexcept -> usize {
  usize hash = 0;
  hash_combine(hash, impl->image);
  hash_combine(hash, impl->image_view);
  hash_combine(hash, impl->sampler);
  return hash;
}

Image::~Image() = default;

void Image::create_mips() {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = impl->image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = aspect_bit;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;
  auto &&[mip_width, mip_height] = properties.extent.as<i32>();

  auto command_buffer = create_immediate(*device, Queue::Type::Graphics);
  transition_image(command_buffer, impl->image, VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   properties.mip_info.mips);
  for (u32 i = 1; i < properties.mip_info.mips; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        command_buffer.get_command_buffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {
        0,
        0,
        0,
    };
    blit.srcOffsets[1] = {
        mip_width,
        mip_height,
        1,
    };
    blit.srcSubresource.aspectMask = aspect_bit;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {
        0,
        0,
        0,
    };
    blit.dstOffsets[1] = {
        mip_width > 1 ? mip_width / 2 : 1,
        mip_height > 1 ? mip_height / 2 : 1,
        1,
    };
    blit.dstSubresource.aspectMask = aspect_bit;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(command_buffer.get_command_buffer(), impl->image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = to_vulkan_layout(properties.layout);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer.get_command_buffer(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mip_width > 1) {
      mip_width /= 2;
    }
    if (mip_height > 1) {
      mip_height /= 2;
    }
  }

  barrier.subresourceRange.baseMipLevel = properties.mip_info.mips - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = to_vulkan_layout(properties.layout);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(command_buffer.get_command_buffer(),
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

auto Image::initialise_vulkan_image() -> void {
  Allocator allocator{"Image"};
  VkImageCreateInfo image_create_info{};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = to_vulkan_format(properties.format);
  image_create_info.extent.width = properties.extent.width;
  image_create_info.extent.height = properties.extent.height;
  image_create_info.extent.depth = 1;
  image_create_info.mipLevels =
      properties.mip_info.valid() ? properties.mip_info.mips : 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = static_cast<VkImageTiling>(properties.tiling);
  image_create_info.usage = static_cast<VkImageUsageFlags>(properties.usage);
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  Creation creation{0};

  static auto is_storage = [&] {
    return (properties.usage & ImageUsage::Storage) != ImageUsage{0};
  };
  if (is_storage()) {
    creation = Creation::HOST_ACCESS_RANDOM_BIT | Creation::MAPPED_BIT;
  }

  impl->allocation = allocator.allocate_image(
      impl->image, impl->allocation_info, image_create_info,
      {
          .usage = Usage::AUTO_PREFER_DEVICE,
          .creation = creation,
      });

  VkImageViewCreateInfo image_view_create_info{};
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.image = impl->image;
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = to_vulkan_format(properties.format);
  image_view_create_info.subresourceRange.aspectMask = aspect_bit;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount =
      properties.mip_info.valid() ? properties.mip_info.mips : 1;
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
  sampler_create_info.anisotropyEnable = VK_TRUE;
  sampler_create_info.maxAnisotropy =
      device->get_device_properties().limits.maxSamplerAnisotropy;
  sampler_create_info.borderColor =
      static_cast<VkBorderColor>(properties.border_color);
  sampler_create_info.unnormalizedCoordinates = VK_FALSE;
  sampler_create_info.mipLodBias = 0.0f;
  sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_create_info.minLod = 0.0f;
  sampler_create_info.maxLod =
      properties.mip_info.valid() ? static_cast<float>(properties.mip_info.mips)
                                  : 1.0f;

  sampler_create_info.compareEnable = VK_FALSE;
  sampler_create_info.compareOp =
      static_cast<VkCompareOp>(properties.compare_op);

  verify(vkCreateSampler(device->get_device(), &sampler_create_info, nullptr,
                         &impl->sampler),
         "vkCreateSampler", "Failed to create sampler");
}

} // namespace Core
