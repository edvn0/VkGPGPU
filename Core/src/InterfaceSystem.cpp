#include "pch/vkgpgpu_pch.hpp"

#include "InterfaceSystem.hpp"

#include "DebugMarker.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Swapchain.hpp"
#include "Verify.hpp"
#include "Window.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace Core {

InterfaceSystem::InterfaceSystem(const Device &dev, const Window &win,
                                 const Swapchain &swap)
    : device(&dev), window(&win), swapchain(&swap) {

  command_executor =
      CommandBuffer::construct(*device, {
                                            .count = 3,
                                            .is_primary = false,
                                            .owned_by_swapchain = false,
                                            .record_stats = false,
                                        });
  std::array<VkDescriptorPoolSize, 11> pool_sizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = static_cast<std::uint32_t>(pool_sizes.size()) * 11ul;
  pool_info.poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes.data();

  verify(
      vkCreateDescriptorPool(device->get_device(), &pool_info, nullptr, &pool),
      "vkCreateDescriptorPool", "Failed to create descriptor pool");
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad
  // Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport /
                                                      // Platform Windows
  // io.ConfigViewportsNoDecoration = false;
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  ImGui::StyleColorsDark();

  ImGuiStyle &style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0F;
    style.Colors[ImGuiCol_WindowBg].w = 1.0F;
  }
  style.Colors[ImGuiCol_WindowBg] =
      ImVec4(0.15f, 0.15f, 0.15f, style.Colors[ImGuiCol_WindowBg].w);

  ImGui_ImplGlfw_InitForVulkan(const_cast<GLFWwindow *>(window->get_native()),
                               true);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = window->get_instance();
  init_info.PhysicalDevice = device->get_physical_device();
  init_info.Device = device->get_device();
  init_info.Queue = device->get_queue(Queue::Type::Graphics);
  init_info.ColorAttachmentFormat = swapchain->get_image_format().format;
  init_info.DescriptorPool = pool;
  init_info.MinImageCount = 2;
  init_info.ImageCount = swapchain->frame_count();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info, swapchain->get_renderpass());

  {
    FS::for_each_in_directory(
        std::filesystem::path{"Assets/Fonts"},
        [&fonts = io.Fonts](const auto &entry) {
          static constexpr auto font_sizes =
              std::array{16.F, 15.F, 14.F, 13.F, 12.F, 11.F, 10.F, 9.F};
          for (const auto &size : font_sizes) {
            fonts->AddFontFromFileTTF(entry.path().string().c_str(), size);
          }
        },
        [](const std::filesystem::directory_entry &entry) {
          return entry.path().extension() == ".ttf";
        });
  }
}

auto InterfaceSystem::begin_frame() -> void {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

auto InterfaceSystem::end_frame() -> void {
  ImGui::Render();
  static constexpr VkClearColorValue clear_colour{
      {
          0.0F,
          0.0F,
          0.0F,
          0.0F,
      },
  };
  static constexpr VkClearDepthStencilValue depth_stencil_clear{
      .depth = 1.0F,
      .stencil = 0,
  };

  std::array<VkClearValue, 2> clear_values{};
  clear_values[0].color = clear_colour;
  clear_values[1].depthStencil = depth_stencil_clear;

  const auto &[width, height] = swapchain->get_extent();

  auto draw_command_buffer = swapchain->get_drawbuffer();

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pNext = nullptr;
  vkBeginCommandBuffer(draw_command_buffer, &begin_info);

  auto *vk_render_pass = swapchain->get_renderpass();
  auto *vk_framebuffer = swapchain->get_framebuffer();

  VkRenderPassBeginInfo render_pass_begin_info = {};
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.renderPass = vk_render_pass;
  render_pass_begin_info.renderArea.offset.x = 0;
  render_pass_begin_info.renderArea.offset.y = 0;
  render_pass_begin_info.renderArea.extent.width = width;
  render_pass_begin_info.renderArea.extent.height = height;
  render_pass_begin_info.clearValueCount =
      static_cast<std::uint32_t>(clear_values.size());
  render_pass_begin_info.pClearValues = clear_values.data();
  render_pass_begin_info.framebuffer = vk_framebuffer;

  DebugMarker::begin_region(draw_command_buffer, "Interface", {1, 0, 0, 1});

  vkCmdBeginRenderPass(draw_command_buffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

  {
    VkCommandBufferInheritanceInfo inheritance_info = {};
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.renderPass = vk_render_pass;
    inheritance_info.framebuffer = vk_framebuffer;

    VkCommandBufferBeginInfo cbi = {};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    cbi.pInheritanceInfo = &inheritance_info;
    command_executor->begin(swapchain->current_frame(), cbi);

    const auto &command_buffer = command_executor->get_command_buffer();
    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.height = static_cast<float>(height);
    viewport.width = static_cast<float>(width);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = width;
    scissor.extent.height = height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    command_executor->end();
  }

  std::array buffer{command_executor->get_command_buffer()};
  vkCmdExecuteCommands(draw_command_buffer, 1, buffer.data());

  DebugMarker::end_region(draw_command_buffer);
  vkCmdEndRenderPass(draw_command_buffer);

  while (!frame_end_callbacks.empty()) {
    auto front = frame_end_callbacks.front();
    frame_end_callbacks.pop();
    front(*command_executor);
  }

  verify(vkEndCommandBuffer(draw_command_buffer), "vkEndCommandBuffer",
         "Failed to end command buffer");

  if (const ImGuiIO &imgui_io = ImGui::GetIO();
      imgui_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}
InterfaceSystem::~InterfaceSystem() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

} // namespace Core
