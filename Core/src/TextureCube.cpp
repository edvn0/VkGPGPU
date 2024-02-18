#include "pch/vkgpgpu_pch.hpp"

#include "TextureCube.hpp"

#include "Allocator.hpp"
#include "CommandBuffer.hpp"
#include "Verify.hpp"

namespace Core {

namespace {
auto calculate_mips(const Extent<u32> &extent) -> u32 {
  auto max_dim = std::max(extent.width, extent.height);
  return static_cast<u32>(std::floor(std::log2(max_dim))) + 1;
}

void insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image,
                                 VkAccessFlags srcAccessMask,
                                 VkAccessFlags dstAccessMask,
                                 VkImageLayout oldImageLayout,
                                 VkImageLayout newImageLayout,
                                 VkPipelineStageFlags srcStageMask,
                                 VkPipelineStageFlags dstStageMask,
                                 VkImageSubresourceRange subresourceRange) {
  VkImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  imageMemoryBarrier.srcAccessMask = srcAccessMask;
  imageMemoryBarrier.dstAccessMask = dstAccessMask;
  imageMemoryBarrier.oldLayout = oldImageLayout;
  imageMemoryBarrier.newLayout = newImageLayout;
  imageMemoryBarrier.image = image;
  imageMemoryBarrier.subresourceRange = subresourceRange;

  vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);
}

bool transition_image(const ImmediateCommandBuffer &buffer,
                      VkImage &to_transition, VkImageLayout from,
                      VkImageLayout to,
                      VkImageSubresourceRange subresourceRange = {
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          0,
                          1,
                          0,
                          1,
                      }) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = to_transition;
  barrier.subresourceRange = subresourceRange;
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
  } else if (from == VK_IMAGE_LAYOUT_UNDEFINED &&
             to == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
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

} // namespace

struct TextureCube::TextureImageImpl {
  const Device *device;
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
  VkImage image{};
  VkImageView image_view{};
  VkSampler sampler{};

  explicit TextureImageImpl(const Device *dev) : device(dev) {}

  ~TextureImageImpl() {
    vkDestroySampler(device->get_device(), sampler, nullptr);
    vkDestroyImageView(device->get_device(), image_view, nullptr);
    Allocator allocator{"Image"};
    allocator.deallocate_image(allocation, image);
  }
};

auto TextureCube::get_descriptor_info() const -> const VkDescriptorImageInfo & {
  return descriptor_info;
}

auto TextureCube::generate_mips(bool readonly) -> void {
  auto command_buffer = create_immediate(*device, Queue::Type::Graphics);

  auto extent_as_i32 = extent.as<i32>();
  uint32_t mipLevels = calculate_mips(extent);
  for (uint32_t face = 0; face < 6; face++) {
    VkImageSubresourceRange mip_subrange = {};
    mip_subrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    mip_subrange.baseMipLevel = 0;
    mip_subrange.baseArrayLayer = face;
    mip_subrange.levelCount = 1;
    mip_subrange.layerCount = 1;

    insert_image_memory_barrier(
        command_buffer.get_command_buffer(), impl->image, 0,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, mip_subrange);
  }

  for (uint32_t i = 1; i < mipLevels; i++) {
    for (uint32_t face = 0; face < 6; face++) {
      VkImageBlit image_blit{};

      image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_blit.srcSubresource.layerCount = 1;
      image_blit.srcSubresource.mipLevel = i - 1;
      image_blit.srcSubresource.baseArrayLayer = face;
      image_blit.srcOffsets[1].x = int32_t(extent_as_i32.width >> (i - 1));
      image_blit.srcOffsets[1].y = int32_t(extent_as_i32.height >> (i - 1));
      image_blit.srcOffsets[1].z = 1;

      image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_blit.dstSubresource.layerCount = 1;
      image_blit.dstSubresource.mipLevel = i;
      image_blit.dstSubresource.baseArrayLayer = face;
      image_blit.dstOffsets[1].x = int32_t(extent_as_i32.width >> i);
      image_blit.dstOffsets[1].y = int32_t(extent_as_i32.height >> i);
      image_blit.dstOffsets[1].z = 1;

      VkImageSubresourceRange mip_subrange = {};
      mip_subrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      mip_subrange.baseMipLevel = i;
      mip_subrange.baseArrayLayer = face;
      mip_subrange.levelCount = 1;
      mip_subrange.layerCount = 1;

      insert_image_memory_barrier(
          command_buffer.get_command_buffer(), impl->image, 0,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT, mip_subrange);

      vkCmdBlitImage(command_buffer.get_command_buffer(), impl->image,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit,
                     VK_FILTER_LINEAR);

      insert_image_memory_barrier(
          command_buffer.get_command_buffer(), impl->image,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT, mip_subrange);
    }
  }

  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.layerCount = 6;
  subresourceRange.levelCount = mipLevels;

  insert_image_memory_barrier(
      command_buffer.get_command_buffer(), impl->image,
      VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      readonly ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_GENERAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      subresourceRange);

  mips_generated = true;

  vkDeviceWaitIdle(device->get_device());

  descriptor_info.imageLayout = readonly
                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_GENERAL;
}

auto TextureCube::construct(const Device &device, ImageFormat format,
                            const Extent<u32> &extent) -> Ref<TextureCube> {
  return Ref<TextureCube>{new TextureCube{device, format, extent}};
}

TextureCube::TextureCube(const Device &dev, ImageFormat format,
                         const Extent<u32> &ext)
    : device(&dev), format(format), extent(ext) {
  impl = make_scope<TextureImageImpl>(device);
  create_empty_texture_cube();
}

auto TextureCube::destroy() -> void {
  if (impl->image != VK_NULL_HANDLE && impl->image_view != VK_NULL_HANDLE &&
      impl->sampler != VK_NULL_HANDLE) {
    impl.reset();
  }
}

auto TextureCube::create_empty_texture_cube() -> void {
  auto vulkanDevice = device->get_device();

  destroy();

  VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
  uint32_t mipCount = calculate_mips(extent);

  VkMemoryAllocateInfo memAllocInfo{};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

  Allocator allocator("TextureCube");

  VkImageCreateInfo imageCreateInfo{};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = format;
  imageCreateInfo.mipLevels = mipCount;
  imageCreateInfo.arrayLayers = 6;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.extent = {
      extent.width,
      extent.height,
      1,
  };
  imageCreateInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  impl->allocation = allocator.allocate_image(
      impl->image, impl->allocation_info, imageCreateInfo,
      {
          .usage = Usage::AUTO_PREFER_DEVICE,
      });

  descriptor_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  {
    const auto command_buffer =
        create_immediate(*device, Queue::Type::Graphics);
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipCount;
    subresourceRange.layerCount = 6;

    transition_image(command_buffer, impl->image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_GENERAL, subresourceRange);
  }

  VkSamplerCreateInfo sampler{};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.maxAnisotropy = 1.0f;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler.mipLodBias = 0.0f;
  sampler.compareOp = VK_COMPARE_OP_NEVER;
  sampler.minLod = 0.0f;
  sampler.maxLod = static_cast<float>(mipCount);

  sampler.maxAnisotropy = 1.0;
  sampler.anisotropyEnable = VK_FALSE;
  sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  verify(vkCreateSampler(vulkanDevice, &sampler, nullptr, &impl->sampler),
         "vkCreateSampler", "Could not create sampler");

  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  view.format = format;
  view.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view.subresourceRange.baseMipLevel = 0;
  view.subresourceRange.baseArrayLayer = 0;
  view.subresourceRange.layerCount = 6;
  view.subresourceRange.levelCount = mipCount;
  view.image = impl->image;
  verify(vkCreateImageView(vulkanDevice, &view, nullptr, &impl->image_view),
         "vkCreateImageView", "Could not create image view");

  descriptor_info.sampler = impl->sampler;
  descriptor_info.imageView = impl->image_view;
  descriptor_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

} // namespace Core
