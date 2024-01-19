#pragma once

#include "Config.hpp"
#include "Device.hpp"
#include "Types.hpp"
#include "Verify.hpp"
#include "Window.hpp"

#include <vulkan/vulkan.h>

namespace Core {

struct SwapchainProperties {
  Extent<u32> extent{};
  u32 image_count{Config::frame_count};
  bool headless{false};
};

class Swapchain {
  static constexpr auto invalid_u32 = ~u32();

public:
  ~Swapchain();

  auto begin_frame() -> void;
  auto end_frame() -> void;
  auto present() -> void;

  [[nodiscard]] auto current_frame() const -> u32;
  [[nodiscard]] auto current_image() const -> u32;
  [[nodiscard]] auto get_drawbuffer() const -> VkCommandBuffer;
  [[nodiscard]] auto get_device() const -> const Device &;
  [[nodiscard]] auto get_swapchain() const -> VkSwapchainKHR;

  [[nodiscard]] auto get_image_format() const -> VkSurfaceFormatKHR;
  [[nodiscard]] auto get_renderpass() const -> VkRenderPass;

  [[nodiscard]] auto get_extent() const -> Extent<u32> {
    return properties.extent;
  }
  [[nodiscard]] auto get_framebuffer(u32 frame = invalid_u32) const
      -> VkFramebuffer {
    if (frame == invalid_u32) {
      frame = frame_index;
    }
    return framebuffers.at(frame);
  }

  static auto construct(const Device &, const Window &,
                        const SwapchainProperties &) -> Scope<Swapchain>;

  auto frame_count() const -> u32;

private:
  Swapchain(const Device &, const Window &, const SwapchainProperties &);
  const Device *device{};
  const Window *window{};

  VkSwapchainKHR swapchain{};
  SwapchainProperties properties{};

  VkSurfaceFormatKHR surface_format{};

  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
  std::vector<VkFence> render_finished_fences;
  std::vector<VkSemaphore> image_available_semaphores;
  std::vector<VkSemaphore> render_finished_semaphores;
  std::vector<VkFramebuffer> framebuffers;
  VkRenderPass renderpass{};

  struct SwapchainCommandBuffer {
    VkCommandBuffer command_buffer{};
    VkCommandPool command_pool{};
  };
  std::vector<SwapchainCommandBuffer> command_buffers;

  u32 frame_index{0};
  u32 current_image_index{0};
};

} // namespace Core
