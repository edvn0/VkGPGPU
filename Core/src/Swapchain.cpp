#include "pch/vkgpgpu_pch.hpp"

#include "Swapchain.hpp"

#include "Logger.hpp"

namespace Core {

static constexpr auto min_image_count =
    [](const VkSurfaceCapabilitiesKHR &capabilities,
       const SwapchainProperties &props) {
      if (capabilities.maxImageCount == 0) {
        const auto found_max =
            std::max(props.image_count, capabilities.minImageCount);
        return std::min(found_max, Config::frame_count);
      } else {
        const auto found_max =
            std::clamp(props.image_count + 1, capabilities.minImageCount,
                       capabilities.maxImageCount);
        return std::min(found_max, Config::frame_count);
      }
    };

static constexpr auto supported_and_preferred_format =
    [](const Device &device, VkSurfaceKHR surface) {
      const auto formats = device.get_physical_device_surface_formats(surface);
      for (const auto &format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          return format;
        }
      }
      return formats[0];
    };

static constexpr auto supported_and_preferred_present_mode =
    [](const Device &device, VkSurfaceKHR surface) {
      const auto present_modes =
          device.get_physical_device_surface_present_modes(surface);
      for (const auto &present_mode : present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
          return present_mode;
        }
      }
      return VK_PRESENT_MODE_FIFO_KHR;
    };

Swapchain::Swapchain(const Core::Device &dev, const Core::Window &win,
                     const SwapchainProperties &props)
    : device(&dev), window(&win), properties(props) {
  // Initialize Vulkan objects
  recreate(window->get_extent(), false);
}

Swapchain::~Swapchain() { destroy(true); }

auto Swapchain::recreate(const Extent<u32> &extent, bool should_clean) -> void {
  vkDeviceWaitIdle(device->get_device());

  properties.extent = extent;

  if (should_clean) {
    destroy(false);
  }

  const auto capabilities = [&] {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device->get_physical_device(), window->get_surface(), &capabilities);
    return capabilities;
  }();

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = window->get_surface();
  const auto count = min_image_count(capabilities, properties);
  create_info.minImageCount = properties.image_count;
  if (properties.image_count != count) {
    warn("Requested image count of {} is not supported, using {} instead",
         count, properties.image_count);
  }

  auto format_and_color_space =
      supported_and_preferred_format(*device, window->get_surface());
  surface_format = format_and_color_space;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = {
      extent.width,
      extent.height,
  };
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode =
      supported_and_preferred_present_mode(*device, window->get_surface());
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = swapchain == VK_NULL_HANDLE ? nullptr : swapchain;
  create_info.pQueueFamilyIndices = nullptr;
  create_info.queueFamilyIndexCount = 0;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  std::array queue_family_indices = {
      device->get_family_index(Queue::Type::Graphics).value(),
      device->get_family_index(Queue::Type::Present).value(),
  };

  if (queue_family_indices[0] != queue_family_indices[1]) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount =
        static_cast<u32>(queue_family_indices.size());
    create_info.pQueueFamilyIndices = queue_family_indices.data();
  }
  vkCreateSwapchainKHR(device->get_device(), &create_info, nullptr, &swapchain);

  if (create_info.oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device->get_device(), create_info.oldSwapchain,
                          nullptr);
  }

  images = [&] {
    u32 count = 0;
    vkGetSwapchainImagesKHR(device->get_device(), swapchain, &count, nullptr);
    std::vector<VkImage> temporary_images(count);
    vkGetSwapchainImagesKHR(device->get_device(), swapchain, &count,
                            temporary_images.data());
    return temporary_images;
  }();

  image_views = [&] {
    std::vector<VkImageView> temporary_image_views;
    temporary_image_views.reserve(images.size());
    for (const auto &image : images) {
      VkImageViewCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = image;
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      create_info.format = format_and_color_space.format;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;
      VkImageView image_view;
      vkCreateImageView(device->get_device(), &create_info, nullptr,
                        &image_view);
      temporary_image_views.push_back(image_view);
    }
    return temporary_image_views;
  }();

  render_finished_fences.resize(properties.image_count);
  image_available_semaphores.resize(properties.image_count);
  render_finished_semaphores.resize(properties.image_count);
  command_buffers.resize(properties.image_count);

  for (auto i = 0; i < properties.image_count; ++i) {
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device->get_device(), &fence_create_info, nullptr,
                  &render_finished_fences[i]);

    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device->get_device(), &semaphore_create_info, nullptr,
                      &image_available_semaphores[i]);

    vkCreateSemaphore(device->get_device(), &semaphore_create_info, nullptr,
                      &render_finished_semaphores[i]);

    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex =
        device->get_family_index(Queue::Type::Graphics).value();
    vkCreateCommandPool(device->get_device(), &command_pool_create_info,
                        nullptr, &command_buffers[i].command_pool);

    VkCommandBufferAllocateInfo command_buffer_allocate_info{};
    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = command_buffers[i].command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(device->get_device(),
                             &command_buffer_allocate_info,
                             &command_buffers[i].command_buffer);
  }

  // Create renderpass
  // Render Pass
  VkAttachmentDescription colour_attachment_description = {};
  // Color attachment
  colour_attachment_description.format = format_and_color_space.format;
  colour_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
  colour_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colour_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colour_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colour_attachment_description.stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colour_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colour_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colour_reference = {};
  colour_reference.attachment = 0;
  colour_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass_description = {};
  subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_description.colorAttachmentCount = 1;
  subpass_description.pColorAttachments = &colour_reference;
  subpass_description.inputAttachmentCount = 0;
  subpass_description.pInputAttachments = nullptr;
  subpass_description.preserveAttachmentCount = 0;
  subpass_description.pPreserveAttachments = nullptr;
  subpass_description.pResolveAttachments = nullptr;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &colour_attachment_description;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass_description;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  vkCreateRenderPass(device->get_device(), &render_pass_info, nullptr,
                     &renderpass);

  framebuffers.resize(image_views.size());

  for (auto i = 0U; i < image_views.size(); ++i) {
    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = renderpass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.pAttachments = &image_views[i];
    framebuffer_create_info.width = extent.width;
    framebuffer_create_info.height = extent.height;
    framebuffer_create_info.layers = 1;
    vkCreateFramebuffer(device->get_device(), &framebuffer_create_info, nullptr,
                        &framebuffers[i]);
  }

  vkDeviceWaitIdle(device->get_device());
}

auto Swapchain::destroy(bool destroy_swapchain) -> void {
  if (destroy_swapchain) {
    vkDestroySwapchainKHR(device->get_device(), swapchain, nullptr);
  }

  for (const auto &image_view : image_views) {
    vkDestroyImageView(device->get_device(), image_view, nullptr);
  }

  for (const auto &command_buffer : command_buffers) {
    vkDestroyCommandPool(device->get_device(), command_buffer.command_pool,
                         nullptr);
  }

  for (const auto &render_finished_fence : render_finished_fences) {
    vkDestroyFence(device->get_device(), render_finished_fence, nullptr);
  }

  for (const auto &image_available_semaphore : image_available_semaphores) {
    vkDestroySemaphore(device->get_device(), image_available_semaphore,
                       nullptr);
  }

  for (const auto &render_finished_semaphore : render_finished_semaphores) {
    vkDestroySemaphore(device->get_device(), render_finished_semaphore,
                       nullptr);
  }

  for (const auto &framebuffer : framebuffers) {
    vkDestroyFramebuffer(device->get_device(), framebuffer, nullptr);
  }

  vkDestroyRenderPass(device->get_device(), renderpass, nullptr);

  info("Destroyed Swapchain!");
}

auto Swapchain::begin_frame() -> bool {
  verify(vkWaitForFences(device->get_device(), 1,
                         &render_finished_fences[current_frame()], VK_TRUE,
                         std::numeric_limits<u64>::max()),
         "vkWaitForFences", "Failed to wait for fences");

  u32 image_index;
  auto result =
      vkAcquireNextImageKHR(device->get_device(), swapchain, UINT64_MAX,
                            image_available_semaphores[current_frame()],
                            VK_NULL_HANDLE, &image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate(window->get_framebuffer_size(), true);
    info("Recreation called from acquire");
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    info("Result: {}", result);
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  verify(vkResetFences(device->get_device(), 1,
                       &render_finished_fences[current_frame()]),
         "vkResetFences", "Failed to reset fences");

  current_image_index = image_index;

  verify(vkResetCommandPool(device->get_device(),
                            command_buffers[current_frame()].command_pool, 0),
         "vkResetCommandPool", "Failed to reset command pool");
  return true;
}

auto Swapchain::end_frame() -> void {
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame()]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers =
      &command_buffers[current_frame()].command_buffer;

  VkSemaphore signal_semaphores[] = {
      render_finished_semaphores[current_frame()]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;

  verify(vkQueueSubmit(device->get_queue(Queue::Type::Graphics), 1,
                       &submit_info, render_finished_fences[current_frame()]),
         "vkQueueSubmit", "Failed to submit draw command buffer");
}

auto Swapchain::present() -> void {
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_semaphores[current_frame()];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &current_image_index;
  present_info.pResults = nullptr;
  const auto &present_queue = device->get_queue(Queue::Type::Graphics);
  verify(vkWaitForFences(device->get_device(), 1,
                         &render_finished_fences[current_frame()], VK_TRUE,
                         UINT64_MAX),
         "vkWaitForFences", "Failed to wait for fences");
  auto result = vkQueuePresentKHR(present_queue, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    recreate(window->get_framebuffer_size(), true);
    info("Recreation called from present");
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  frame_index = (frame_index + 1) % properties.image_count;
  verify(vkWaitForFences(device->get_device(), 1,
                         &render_finished_fences[current_frame()], VK_TRUE,
                         UINT64_MAX),
         "vkWaitForFences", "Failed to wait for fences");
}

auto Swapchain::construct(const Device &device, const Window &window,
                          const SwapchainProperties &props)
    -> Scope<Swapchain> {
  return Scope<Swapchain>(new Swapchain(device, window, props));
}

auto Swapchain::get_drawbuffer() const -> VkCommandBuffer {
  return command_buffers[current_frame()].command_buffer;
}

auto Swapchain::frame_count() const -> u32 { return properties.image_count; }

auto Swapchain::get_device() const -> const Device & { return *device; }

auto Swapchain::current_frame() const -> u32 { return frame_index; }

auto Swapchain::current_image() const -> u32 { return current_image_index; }

auto Swapchain::get_swapchain() const -> VkSwapchainKHR { return swapchain; }

auto Swapchain::get_image_format() const -> VkSurfaceFormatKHR {
  return surface_format;
}

auto Swapchain::get_renderpass() const -> VkRenderPass { return renderpass; }

} // namespace Core
