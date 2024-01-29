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

auto Framebuffer::on_resize(const Extent<u32> &new_extent) -> void {
  return on_resize(new_extent.width, new_extent.height, true);
}

auto Framebuffer::on_resize(u32 w, u32 h, bool should_clean) -> void {
  if (w == properties.width && h == properties.height) {
    return;
  }

  properties.width = w;
  properties.height = h;
  width = w;
  height = h;

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

  std::vector<VkAttachmentReference> color_attachment_references;
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
        const auto &existing_framebuffer = properties.existing_framebuffer;
        depth_attachment_image = existing_framebuffer->get_depth_image();
      } else if (properties.existing_images.contains(attachment_index)) {
        const auto &existing_image =
            properties.existing_images.at(attachment_index);
        depth_attachment_image = existing_image;
      } else {
        if (depth_attachment_image) {
          auto &spec = depth_attachment_image->get_properties();
          spec.format = attachment_specification.format;
          spec.usage = ImageUsage::DepthStencilAttachment;
          spec.layout = ImageLayout::DepthStencilReadOnlyOptimal;
          spec.extent.width = width * properties.scale;
          spec.extent.height = height * properties.scale;
        } else {
          depth_attachment_image = Image::construct_reference(
              *device,
              {
                  .extent =
                      {
                          .width = static_cast<u32>(width * properties.scale),
                          .height = static_cast<u32>(height * properties.scale),
                      },
                  .format = attachment_specification.format,
                  .usage =
                      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled,
                  .layout = ImageLayout::DepthStencilReadOnlyOptimal,
                  .address_mode = SamplerAddressMode::ClampToBorder,
                  .border_color = SamplerBorderColor::FloatOpaqueWhite,
                  .compare_op = CompareOperation::Less,
              });
        }
      }

      VkAttachmentDescription &attachment_description =
          attachmentDescriptions.emplace_back();
      attachment_description.flags = 0;
      attachment_description.format =
          to_vulkan_format(attachment_specification.format);
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp = properties.clear_depth_on_load
                                          ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                          : VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.initialLayout =
          properties.clear_depth_on_load
              ? VK_IMAGE_LAYOUT_UNDEFINED
              : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      attachment_description.finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      attachment_description.finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      depthAttachmentReference = {
          attachment_index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
      clear_values[attachment_index].depthStencil = {
          .depth = properties.depth_clear_value,
          .stencil = 0,
      };
    } else {
      Ref<Image> color_attachment;
      if (properties.existing_framebuffer) {
        const auto &existing_framebuffer = properties.existing_framebuffer;
        auto &existing_image =
            existing_framebuffer->get_image(attachment_index);
        color_attachment = attachment_images.emplace_back(existing_image);
      } else if (properties.existing_images.contains(attachment_index)) {
        auto &existing_image = properties.existing_images[attachment_index];
        color_attachment = existing_image;
        attachment_images[attachment_index] = existing_image;
      } else {
        if (create_images) {
          ImageProperties spec{};
          spec.format = attachment_specification.format;
          spec.min_filter = SamplerFilter::Nearest,
          spec.max_filter = SamplerFilter::Nearest,
          spec.layout = ImageLayout::ShaderReadOnlyOptimal;
          spec.usage = ImageUsage::ColourAttachment | ImageUsage::Sampled |
                       ImageUsage::TransferSrc | ImageUsage::TransferDst;
          spec.extent.width = static_cast<u32>(width * properties.scale);
          spec.extent.height = static_cast<u32>(height * properties.scale);
          color_attachment = attachment_images.emplace_back(
              Image::construct_reference(*device, spec));
        } else {
          auto &image = attachment_images[attachment_index];
          ImageProperties &spec = image->get_properties();
          spec.layout = ImageLayout::ShaderReadOnlyOptimal;
          spec.extent.width = static_cast<u32>(width * properties.scale);
          spec.extent.height = static_cast<u32>(height * properties.scale);
          spec.format = attachment_specification.format;
          color_attachment = image;
          if (attachment_index == 0 &&
              properties.existing_image_layers[0] == 0) {
            color_attachment->recreate();
            // color_attachment->RT_CreatePerSpecificLayerImageViews(
            //    properties.existing_imageLayers);
          } else if (attachment_index == 0) {
            // color_attachment->RT_CreatePerSpecificLayerImageViews(
            //     properties.existing_imageLayers);
          } else {
            color_attachment->recreate();
          }
        }
      }

      VkAttachmentDescription &attachment_description =
          attachmentDescriptions.emplace_back();
      attachment_description.flags = 0;
      attachment_description.format =
          to_vulkan_format(attachment_specification.format);
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp = properties.clear_colour_on_load
                                          ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                          : VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.initialLayout =
          properties.clear_colour_on_load
              ? VK_IMAGE_LAYOUT_UNDEFINED
              : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      attachment_description.finalLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      const auto &clear_colour = properties.clear_colour;
      clear_values[attachment_index].color = {{
          clear_colour.r,
          clear_colour.g,
          clear_colour.b,
          clear_colour.a,
      }};
      color_attachment_references.emplace_back(VkAttachmentReference{
          attachment_index,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
    }

    attachment_index++;
  }

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount =
      static_cast<u32>(color_attachment_references.size());
  subpassDescription.pColorAttachments = color_attachment_references.data();
  if (depth_attachment_image) {
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
  }

  std::vector<VkSubpassDependency> dependencies;

  if (attachment_images.size()) {
    {
      VkSubpassDependency &dependency = dependencies.emplace_back();
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
    {
      VkSubpassDependency &dependency = dependencies.emplace_back();
      dependency.srcSubpass = 0;
      dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
  }

  if (depth_attachment_image) {
    {
      VkSubpassDependency &dependency = dependencies.emplace_back();
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    {
      VkSubpassDependency &dependency = dependencies.emplace_back();
      dependency.srcSubpass = 0;
      dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
      dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
  }

  // Create the actual renderpass
  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount =
      static_cast<u32>(attachmentDescriptions.size());
  render_pass_info.pAttachments = attachmentDescriptions.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpassDescription;
  render_pass_info.dependencyCount = static_cast<u32>(dependencies.size());
  render_pass_info.pDependencies = dependencies.data();

  verify(vkCreateRenderPass(device->get_device(), &render_pass_info, nullptr,
                            &render_pass),
         "vkCreateRenderPass", "Failed to create render pass!");
  DebugMarker::set_object_name(*device, render_pass,
                               VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT,
                               properties.debug_name.c_str());
  std::vector<VkImageView> view_attachments(attachment_images.size());
  for (u32 i = 0; i < attachment_images.size(); i++) {
    auto &image = attachment_images[i];
    view_attachments[i] = image->get_descriptor_info().imageView;
  }

  if (depth_attachment_image) {
    auto &image = depth_attachment_image;
    view_attachments.emplace_back(image->get_descriptor_info().imageView);
  }

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = render_pass;
  framebufferCreateInfo.attachmentCount =
      static_cast<u32>(view_attachments.size());
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

  info("Created Framebuffer '{}'. Size: {}x{}", properties.debug_name, width,
       height);
}

} // namespace Core
