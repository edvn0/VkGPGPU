#include "pch/vkgpgpu_pch.hpp"

#include "Framebuffer.hpp"

#include "Allocator.hpp"
#include "DebugMarker.hpp"
#include "Verify.hpp"

namespace Core {

static constexpr auto is_depth_format(ImageFormat format) -> bool {
  switch (format) {
  case ImageFormat::DEPTH16:
  case ImageFormat::DEPTH32F:
  case ImageFormat::DEPTH24STENCIL8:
    return true;
  default:
    return false;
  }
}

auto Framebuffer::construct(const Device &device,
                            const FramebufferProperties &properties)
    -> Scope<Framebuffer> {
  return Scope<Framebuffer>(new Framebuffer(device, properties));
}

Framebuffer::Framebuffer(const Device &dev,
                         const FramebufferProperties &properties)
    : device(&dev), properties(properties), width(properties.width),
      height(properties.height) {
  create_framebuffer();
}

Framebuffer::~Framebuffer() {
  if (framebuffer) {
    vkDestroyFramebuffer(device->get_device(), framebuffer, nullptr);
  }
  if (render_pass) {
    vkDestroyRenderPass(device->get_device(), render_pass, nullptr);
  }
  // release other resource manually
  attachment_images.clear();
  depth_attachment_image = nullptr;
  debug("Destroyed Framebuffer '{}'", properties.debug_name);
}

auto Framebuffer::resize(u32 w, u32 h, bool should_clean) -> void {
  if (w == properties.width && h == properties.height) {
    return;
  }

  properties.width = w;
  properties.height = h;

  if (should_clean) {
    clean();
  }

  create_framebuffer();
}

auto Framebuffer::clean() -> void {
  clear_values.clear();
  attachment_images.clear();
  depth_attachment_image = nullptr;
  if (framebuffer) {
    vkDestroyFramebuffer(device->get_device(), framebuffer, nullptr);
  }
  if (render_pass) {
    vkDestroyRenderPass(device->get_device(), render_pass, nullptr);
  }
}

auto Framebuffer::add_resize_callback(const resize_callback &func) -> void {
  resize_callbacks.push_back(func);
}

auto Framebuffer::invalidate() -> void {
  clean();
  create_framebuffer();
}

auto Framebuffer::create_framebuffer() -> void {
  trace("Framebuffer::create_framebuffer");
  Allocator allocator("Framebuffer");

  std::vector<VkAttachmentDescription> attachmentDescriptions;

  std::vector<VkAttachmentReference> color_attachmentReferences;
  VkAttachmentReference depthAttachmentReference;

  const auto &attachments = properties.attachments.attachments;
  clear_values.resize(attachments.size());

  bool create_images = attachment_images.empty();

  if (properties.existing_framebuffer)
    attachment_images.clear();

  u32 attachment_index = 0;
  for (auto attachment_specification : attachments) {
    if (is_depth_format(attachment_specification.format)) {
      if (properties.existing_image) {
        depth_attachment_image = properties.existing_image;
      } else if (properties.existing_framebuffer) {
        const auto &existingFramebuffer = properties.existing_framebuffer;
        depth_attachment_image = existingFramebuffer->get_depth_image();
      } else if (properties.existing_images.find(attachment_index) !=
                 properties.existing_images.end()) {
        Ref<Image> existing_image =
            properties.existing_images.at(attachment_index);
        // assert on depth format
        depth_attachment_image = existing_image;
      } else {
        Ref<Image> depthAttachmentImage = depth_attachment_image;
        auto &spec = depthAttachmentImage->get_properties();
        spec.layout = ImageLayout::DepthStencilAttachmentOptimal;
        spec.extent.width = static_cast<u32>(width * properties.scale);
        spec.extent.height = static_cast<u32>(height * properties.scale);
        depthAttachmentImage->recreate();
      }

      VkAttachmentDescription &attachmentDescription =
          attachmentDescriptions.emplace_back();
      attachmentDescription.flags = 0;
      attachmentDescription.format =
          to_vulkan_format(attachment_specification.format);
      attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescription.loadOp = properties.clear_depth_on_load
                                         ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                         : VK_ATTACHMENT_LOAD_OP_LOAD;
      attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachmentDescription.initialLayout =
          properties.clear_depth_on_load
              ? VK_IMAGE_LAYOUT_UNDEFINED
              : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      attachmentDescription.finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      attachmentDescription.finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      depthAttachmentReference = {
          attachment_index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
      clear_values[attachment_index].depthStencil = {
          .depth = properties.depth_clear_value, .stencil = 0};
    } else {
      // HZ_CORE_ASSERT(!properties.existing_image, "Not supported for color
      // attachments");

      Ref<Image> color_attachment;
      if (properties.existing_framebuffer) {
        const auto existingFramebuffer = properties.existing_framebuffer;
        Ref<Image> existing_image =
            existingFramebuffer->get_image(attachment_index);
        color_attachment = attachment_images.emplace_back(existing_image);
      } else if (properties.existing_images.contains(attachment_index)) {
        Ref<Image> existing_image =
            properties.existing_images[attachment_index];
        // assert on depth format
        color_attachment = existing_image;
        attachment_images[attachment_index] = existing_image;
      } else {
        if (create_images) {
          ImageProperties spec{};
          spec.format = attachment_specification.format;
          spec.layout = ImageLayout::ShaderReadOnlyOptimal;
          spec.usage = ImageUsage::ColorAttachment | ImageUsage::Sampled |
                       ImageUsage::TransferSrc | ImageUsage::TransferDst;
          spec.extent.width = static_cast<u32>(width * properties.scale);
          spec.extent.height = static_cast<u32>(height * properties.scale);
          color_attachment = attachment_images.emplace_back(
              Image::construct_reference(*device, spec));
        } else {
          Ref<Image> image = attachment_images[attachment_index];
          ImageProperties &spec = image->get_properties();
          spec.layout = ImageLayout::ShaderReadOnlyOptimal;
          spec.extent.width = static_cast<u32>(width * properties.scale);
          spec.extent.height = static_cast<u32>(height * properties.scale);
          color_attachment = image;
          if (attachment_index == 0 &&
              properties.existing_image_layers[0] == 0) {
            color_attachment->recreate(); // Create immediately
            // color_attachment->RT_CreatePerSpecificLayerImageViews(
            //    properties.existing_imageLayers);
          } else if (attachment_index == 0) {
            // color_attachment->RT_CreatePerSpecificLayerImageViews(
            //     properties.existing_imageLayers);
          } else {
            color_attachment->recreate(); // Create immediately
          }
        }
      }

      VkAttachmentDescription &attachmentDescription =
          attachmentDescriptions.emplace_back();
      attachmentDescription.flags = 0;
      attachmentDescription.format =
          to_vulkan_format(attachment_specification.format);
      attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescription.loadOp = properties.clear_colour_on_load
                                         ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                         : VK_ATTACHMENT_LOAD_OP_LOAD;
      attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachmentDescription.initialLayout =
          properties.clear_colour_on_load
              ? VK_IMAGE_LAYOUT_UNDEFINED
              : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      attachmentDescription.finalLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      const auto &clearColor = properties.clear_colour;
      clear_values[attachment_index].color = {
          {clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
      color_attachmentReferences.emplace_back(VkAttachmentReference{
          attachment_index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    }

    attachment_index++;
  }

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount =
      u32(color_attachmentReferences.size());
  subpassDescription.pColorAttachments = color_attachmentReferences.data();
  if (depth_attachment_image)
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

  std::vector<VkSubpassDependency> dependencies;

  if (attachment_images.size()) {
    {
      VkSubpassDependency &depedency = dependencies.emplace_back();
      depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
      depedency.dstSubpass = 0;
      depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      depedency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      depedency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
    {
      VkSubpassDependency &depedency = dependencies.emplace_back();
      depedency.srcSubpass = 0;
      depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
      depedency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      depedency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
  }

  if (depth_attachment_image) {
    {
      VkSubpassDependency &depedency = dependencies.emplace_back();
      depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
      depedency.dstSubpass = 0;
      depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      depedency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      depedency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    {
      VkSubpassDependency &depedency = dependencies.emplace_back();
      depedency.srcSubpass = 0;
      depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
      depedency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      depedency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
  }

  // Create the actual renderpass
  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount =
      static_cast<u32>(attachmentDescriptions.size());
  renderPassInfo.pAttachments = attachmentDescriptions.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  verify(vkCreateRenderPass(device->get_device(), &renderPassInfo, nullptr,
                            &render_pass),
         "vkCreateRenderPass", "Failed to create render pass!");
  DebugMarker::set_object_name(*device, render_pass,
                               VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT,
                               properties.debug_name.c_str());
  std::vector<VkImageView> view_attachments(attachment_images.size());
  for (u32 i = 0; i < attachment_images.size(); i++) {
    Ref<Image> image = attachment_images[i];
    view_attachments[i] = image->get_descriptor_info().imageView;
  }

  if (depth_attachment_image) {
    Ref<Image> image = depth_attachment_image;
    view_attachments.emplace_back(image->get_descriptor_info().imageView);
  }

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = render_pass;
  framebufferCreateInfo.attachmentCount = u32(view_attachments.size());
  framebufferCreateInfo.pAttachments = view_attachments.data();
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = 1;

  verify(vkCreateFramebuffer(device->get_device(), &framebufferCreateInfo,
                             nullptr, &framebuffer),
         "vkCreateFramebuffer", "Failed to create framebuffer!");
  DebugMarker::set_object_name(*device, framebuffer,
                               VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT,
                               properties.debug_name.c_str());
}

} // namespace Core
