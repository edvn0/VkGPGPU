#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Framebuffer.hpp"
#include "Material.hpp"
#include "UI.hpp"

#include <array>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

template <Core::Buffer::Type T>
auto create_or_get_write_descriptor_for(std::uint32_t frames_in_flight,
                                        Core::BufferSet<T> *buffer_set,
                                        Material &material)
    -> const std::vector<std::vector<VkWriteDescriptorSet>> & {
  static std::unordered_map<
      BufferSet<T> *,
      std::unordered_map<std::size_t,
                         std::vector<std::vector<VkWriteDescriptorSet>>>>
      buffer_set_write_descriptor_cache;

  constexpr auto get_descriptor_set_vector =
      [](auto &map, auto *key, auto hash) -> auto & { return map[key][hash]; };

  const auto &vulkan_shader = material.get_shader();
  const auto shader_hash = vulkan_shader.hash();
  if (buffer_set_write_descriptor_cache.contains(buffer_set)) {
    const auto &shader_map = buffer_set_write_descriptor_cache.at(buffer_set);
    if (shader_map.contains(shader_hash)) {
      const auto &write_descriptors = shader_map.at(shader_hash);
      return write_descriptors;
    }
  }

  if (!vulkan_shader.has_descriptor_set(0) ||
      !vulkan_shader.has_descriptor_set(1)) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_sets =
      vulkan_shader.get_reflection_data().shader_descriptor_sets;
  if (shader_descriptor_sets.empty()) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_set = shader_descriptor_sets[0];
  const auto &storage_buffers = shader_descriptor_set.storage_buffers;
  const auto &uniform_buffers = shader_descriptor_set.uniform_buffers;

  if constexpr (T == Buffer::Type::Storage) {
    for (const auto &binding : storage_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
        write_descriptors[frame].push_back(wds);
      }
    }
  } else {
    for (const auto &binding : uniform_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
        write_descriptors[frame].push_back(wds);
      }
    }
  }

  return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                   buffer_set, shader_hash);
}

auto randomize_span_of_matrices(std::span<Math::Mat4> matrices) -> void {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dis(-1.0F, 1.0F);

  // Randomize span of glm::mat4
  for (auto &mat : matrices) {
    Math::for_each(mat, [&](floating &value) { value = dis(gen); });
  }
}

template <u32 N>
  requires(N % 2 == 1)
consteval auto compute_kernel_size() -> std::tuple<u32, u32, u32> {
  constexpr auto half_size = N / 2;
  constexpr auto kernel_size = N * N;
  constexpr auto center_value = N * N - 1;
  return {kernel_size, half_size, center_value};
}

auto compute_kernel_size(std::integral auto N) -> std::tuple<u32, u32, u32> {
  const auto half_size = N / 2;
  const auto kernel_size = N * N;
  const auto center_value = N * N - 1;
  return {kernel_size, half_size, center_value};
}

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), uniform_buffer_set(*get_device()),
      storage_buffer_set(*get_device()), timer(*get_messaging_client()) {
  auto &&[kernel, half, center] = compute_kernel_size<3>();
  pc.kernel_size = kernel;
  pc.half_size = half;
  pc.center_value = center;
  widgets.emplace_back(make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path()));
};

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  for (const auto &widget : widgets)
    widget->on_update(ts);

  compute(ts);
  graphics(ts);

  timer.end();
}

void ClientApp::on_create() {
  for (const auto &widget : widgets)
    widget->on_create();
  perform();
}

void ClientApp::on_destroy() {
  for (const auto &widget : widgets)
    widget->on_destroy();
  // Destroy all fields
  command_buffer.reset();

  pipeline.reset();
  shader.reset();
  texture.reset();
  output_texture.reset();
}

class SceneRenderer {
public:
  auto begin_renderpass(const CommandBuffer &buffer,
                        const Framebuffer &framebuffer) -> void {
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = framebuffer.get_render_pass();
    render_pass_begin_info.framebuffer = framebuffer.get_framebuffer();
    render_pass_begin_info.renderArea.extent = {
        .width = framebuffer.get_width(),
        .height = framebuffer.get_height(),
    };
    const auto &clear_values = framebuffer.get_clear_values();
    render_pass_begin_info.clearValueCount =
        static_cast<u32>(clear_values.size());
    render_pass_begin_info.pClearValues = clear_values.data();
    vkCmdBeginRenderPass(buffer.get_command_buffer(), &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    // Scissors and viewport
    VkViewport viewport = {};
    viewport.width = static_cast<float>(framebuffer.get_width());
    viewport.height = static_cast<float>(framebuffer.get_height());
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(buffer.get_command_buffer(), 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent.width = framebuffer.get_width();
    scissor.extent.height = framebuffer.get_height();
    vkCmdSetScissor(buffer.get_command_buffer(), 0, 1, &scissor);
  }

  auto end_renderpass(const CommandBuffer &buffer) -> void {
    vkCmdEndRenderPass(buffer.get_command_buffer());

    bound_pipeline.reset();
  }

  struct DrawParameters {
    u32 index_count;
    u32 instance_count{1};
    u32 first_index{0};
    u32 vertex_offset{0};
    u32 first_instance{0};
  };
  auto draw(const CommandBuffer &buffer, const DrawParameters &params) {
    vkCmdDrawIndexed(buffer.get_command_buffer(), params.index_count,
                     params.instance_count, params.first_index,
                     params.vertex_offset, params.first_instance);
  }

  auto bind_pipeline(const CommandBuffer &buffer,
                     const GraphicsPipeline &pipeline) {
    pipeline.bind(buffer);
    bound_pipeline = {
        .bound_pipeline = pipeline.get_pipeline(),
        .hash = pipeline.hash(),
    };
  }

  auto bind_index_buffer(const CommandBuffer &buffer,
                         const Buffer &index_buffer) {
    vkCmdBindIndexBuffer(buffer.get_command_buffer(), index_buffer.get_buffer(),
                         0, VK_INDEX_TYPE_UINT32);
  }

  auto bind_vertex_buffer(const CommandBuffer &buffer,
                          const Buffer &vertex_buffer) {
    const std::array<VkDeviceSize, 1> offset = {0};
    const std::array buffers = {vertex_buffer.get_buffer()};
    vkCmdBindVertexBuffers(buffer.get_command_buffer(), 0, 1, buffers.data(),
                           offset.data());
  }

private:
  struct PipelineAndHash {
    VkPipeline bound_pipeline{nullptr};
    u64 hash{0};

    auto reset() -> void {
      bound_pipeline = nullptr;
      hash = 0;
    }
  };
  PipelineAndHash bound_pipeline{};

  auto is_already_bound(const GraphicsPipeline &pipeline) -> bool {
    return pipeline.hash() == bound_pipeline.hash;
  }
};

auto ClientApp::graphics(floating ts) -> void {
  static SceneRenderer scene_renderer;
  graphics_command_buffer->begin(frame());
  scene_renderer.begin_renderpass(*graphics_command_buffer, *framebuffer);
  scene_renderer.bind_pipeline(*graphics_command_buffer, *graphics_pipeline);

  graphics_material->set("input_image", *output_texture);
  graphics_material->set("output_image", *output_texture_second);
  graphics_material->set("geometry_texture", *output_texture_second);
  update_material_for_rendering(FrameIndex{frame()}, *graphics_material,
                                &uniform_buffer_set, &storage_buffer_set);
  graphics_material->bind(*graphics_command_buffer, *graphics_pipeline,
                          frame());

  scene_renderer.bind_index_buffer(*graphics_command_buffer, *index_buffer);
  scene_renderer.bind_vertex_buffer(*graphics_command_buffer, *vertex_buffer);
  scene_renderer.draw(*graphics_command_buffer, {
                                                    .index_count = 3,
                                                    .instance_count = 1,
                                                });
  scene_renderer.end_renderpass(*graphics_command_buffer);
  graphics_command_buffer->end_and_submit();
}

auto ClientApp::compute(floating ts) -> void {
  randomize_span_of_matrices(matrices);
  const auto &input_buffer =
      storage_buffer_set.get(DescriptorBinding(0), frame(), DescriptorSet(0));
  input_buffer->write(matrices.data(), matrices.size() * sizeof(Math::Mat4));
  randomize_span_of_matrices(matrices);
  const auto &other_input_buffer =
      storage_buffer_set.get(DescriptorBinding(1), frame(), DescriptorSet(0));
  other_input_buffer->write(matrices.data(),
                            matrices.size() * sizeof(Math::Mat4));

  static long double angle = 0.0;
  angle += ts * 0.1;

  // Map angle to 0 to 2pi
  const auto angle_in_radians = std::sin(angle);
  const auto &simple_uniform =
      uniform_buffer_set.get(DescriptorBinding(3), frame(), DescriptorSet(0));
  simple_uniform->write(&angle_in_radians, simple_uniform->get_size());

  material->set("pc.kernelSize", pc.kernel_size);
  material->set("pc.halfSize", pc.half_size);
  material->set("pc.precomputedCenterValue", pc.center_value);
  material->set("input_image", *texture);
  material->set("output_image", *output_texture);
  update_material_for_rendering(FrameIndex{frame()}, *material,
                                &uniform_buffer_set, &storage_buffer_set);

  // Begin command buffer
  command_buffer->begin(frame());
  // Bind pipeline
  DebugMarker::begin_region(command_buffer->get_command_buffer(),
                            "MatrixMultiply",
                            {
                                1.0F,
                                0.0F,
                                0.0F,
                            });
  pipeline->bind(*command_buffer);
  material->bind(*command_buffer, *pipeline, frame());

  constexpr auto wg_size = 16UL;

  // Number of groups in each dimension
  constexpr auto dispatchX = 1024 / wg_size;
  constexpr auto dispatchY = 1024 / wg_size;
  dispatcher->push_constant(*pipeline, *material);
  dispatcher->dispatch({
      .group_count_x = dispatchX,
      .group_count_y = dispatchY,
      .group_count_z = 1,
  });
  // End command buffer
  DebugMarker::end_region(command_buffer->get_command_buffer());
  command_buffer->end_and_submit();

  command_buffer->begin(frame());
  dispatcher->set_command_buffer(command_buffer.get());
  dispatcher->bind(*second_pipeline);

  second_material->set("pc.kernelSize", pc.kernel_size);
  second_material->set("pc.halfSize", pc.half_size);
  second_material->set("pc.precomputedCenterValue", pc.center_value);
  second_material->set("input_image", *output_texture);
  second_material->set("output_image", *output_texture_second);
  update_material_for_rendering(FrameIndex{frame()}, *second_material,
                                &uniform_buffer_set, &storage_buffer_set);

  second_material->bind(*command_buffer, *second_pipeline, frame());

  dispatcher->push_constant(*second_pipeline, *second_material);
  dispatcher->dispatch({
      .group_count_x = dispatchX,
      .group_count_y = dispatchY,
      .group_count_z = 1,
  });

  command_buffer->end_and_submit();
}

void ClientApp::perform() {
  texture = Texture::construct_storage(
      *get_device(),
      {
          .format = ImageFormat::UNORM_RGBA8,
          .path = FS::texture("viking_room.png"),
          .usage = ImageUsage::Sampled | ImageUsage::Storage |
                   ImageUsage::TransferDst | ImageUsage::TransferSrc,
          .layout = ImageLayout::General,
      });
  output_texture = Texture::empty_with_size(
      *get_device(), texture->size_bytes(), texture->get_extent());
  output_texture_second = Texture::empty_with_size(
      *get_device(), texture->size_bytes(), texture->get_extent());

  command_buffer = CommandBuffer::construct(
      *get_device(), CommandBufferProperties{
                         .queue_type = Queue::Type::Compute,
                         .record_stats = true,
                     });
  graphics_command_buffer = CommandBuffer::construct(
      *get_device(), CommandBufferProperties{
                         .queue_type = Queue::Type::Graphics,
                         .record_stats = true,
                     });
  randomize_span_of_matrices(matrices);

  static constexpr auto matrices_size =
      std::array<Math::Mat4, 10>{}.size() * sizeof(Math::Mat4);

  storage_buffer_set.create(matrices_size, SetBinding(0));
  storage_buffer_set.create(matrices_size, SetBinding(1));
  storage_buffer_set.create(matrices_size, SetBinding(2));
  uniform_buffer_set.create(4, SetBinding(3));

  vertex_buffer = Buffer::construct(*get_device(), sizeof(float) * 3 * 3,
                                    Buffer::Type::Vertex);
  vertex_buffer->write(
      std::array{
          -1.0F, -1.0F, 0.0F, // bottom left
          1.0F, -1.0F, 0.0F,  // bottom right
          0.0F, 1.0F, 0.0F,   // top
      }
          .data(),
      sizeof(float) * 3 * 3);
  index_buffer =
      Buffer::construct(*get_device(), sizeof(u32) * 3, Buffer::Type::Index);
  index_buffer->write(std::array{0U, 1U, 2U}.data(), sizeof(u32) * 3);

  {
    shader = Shader::construct(*get_device(),
                               FS::shader("LaplaceEdgeDetection.comp.spv"));

    material = Material::construct(*get_device(), *shader);
    pipeline = Pipeline::construct(*get_device(), PipelineConfiguration{
                                                      "LaplaceEdgeDetection",
                                                      PipelineStage::Compute,
                                                      *shader,
                                                  });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
    material->set("pc.kernelSize", kernel_size);
    material->set("pc.halfSize", half_size);
    material->set("pc.precomputedCenterValue", center_value);
  }
  {
    second_shader = Shader::construct(
        *get_device(), FS::shader("LaplaceEdgeDetection_Second.comp.spv"));

    second_material = Material::construct(*get_device(), *second_shader);
    second_pipeline =
        Pipeline::construct(*get_device(), PipelineConfiguration{
                                               "LaplaceEdgeDetection_Second",
                                               PipelineStage::Compute,
                                               *second_shader,
                                           });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<127>();
    second_material->set("pc.kernelSize", kernel_size);
    second_material->set("pc.halfSize", half_size);
    second_material->set("pc.precomputedCenterValue", center_value);
  }
  {
    FramebufferProperties props{
        .width = get_swapchain()->get_extent().width,
        .height = get_swapchain()->get_extent().height,
        .attachments =
            FramebufferAttachmentSpecification{
                FramebufferTextureSpecification{
                    .format = ImageFormat::SRGB_RGBA8,
                },
            },
        .debug_name = "DefaultFramebuffer",
    };
    framebuffer = Framebuffer::construct(*get_device(), props);

    graphics_shader =
        Shader::construct(*get_device(), FS::shader("Triangle.vert.spv"),
                          FS::shader("Triangle.frag.spv"));

    graphics_material = Material::construct(*get_device(), *graphics_shader);
    graphics_pipeline = GraphicsPipeline::construct(
        *get_device(), GraphicsPipelineConfiguration{
                           .name = "DefaultGraphicsPipeline",
                           .shader = graphics_shader.get(),
                           .framebuffer = framebuffer.get(),
                       });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<127>();
    graphics_material->set("pc.kernelSize", kernel_size);
    graphics_material->set("pc.halfSize", half_size);
    graphics_material->set("pc.precomputedCenterValue", center_value);
  }

  dispatcher = make_scope<CommandDispatcher>(command_buffer.get());
}

void ClientApp::update_material_for_rendering(
    FrameIndex frame_index, Material &material_for_update,
    BufferSet<Buffer::Type::Uniform> *ubo_set,
    BufferSet<Buffer::Type::Storage> *sbo_set) {
  if (ubo_set != nullptr) {
    auto write_descriptors =
        create_or_get_write_descriptor_for<Buffer::Type::Uniform>(
            Config::frame_count, ubo_set, material_for_update);
    if (sbo_set != nullptr) {
      const auto &storage_buffer_write_sets =
          create_or_get_write_descriptor_for<Buffer::Type::Storage>(
              Config::frame_count, sbo_set, material_for_update);

      for (u32 frame = 0; frame < Config::frame_count; frame++) {
        const auto &sbo_ws = storage_buffer_write_sets[frame];
        const auto reserved_size =
            write_descriptors[frame].size() + sbo_ws.size();
        write_descriptors[frame].reserve(reserved_size);
        write_descriptors[frame].insert(write_descriptors[frame].end(),
                                        sbo_ws.begin(), sbo_ws.end());
      }
    }
    material_for_update.update_for_rendering(frame_index, write_descriptors);
  } else {
    material_for_update.update_for_rendering(frame_index);
  }
}

void ClientApp::on_interface(InterfaceSystem &system) {

  // Create a fullscreen window for the dockspace
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

  window_flags &= ~ImGuiWindowFlags_MenuBar;

  // Create the window that will contain the dockspace
  if (ImGui::Begin("DockSpace Demo", nullptr, window_flags)) {
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    // Here you can add your own windows, for example:
    static constexpr auto draw_stats =
        [](const auto &average,
           const auto &command_buffer_with_compute_stats_available) {
          // Retrieve statistics from the timer and command buffer
          auto &&[frametime, fps] = average.get_statistics();
          auto &&[compute_times] =
              command_buffer_with_compute_stats_available.get_statistics();

          // Start a new ImGui table
          if (ImGui::BeginTable("StatsTable", 2)) {
            // Set up column headers
            ImGui::TableSetupColumn("Statistic");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            // Row for frametime
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            UI::text("Frametime");
            ImGui::TableSetColumnIndex(1);
            UI::text("{} ms", frametime);

            // Row for FPS
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            UI::text("FPS");
            ImGui::TableSetColumnIndex(1);
            UI::text("{}", fps);

            // Row for Compute Time
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            UI::text("Compute Time");
            ImGui::TableSetColumnIndex(1);
            UI::text("{} ms", compute_times);

            // End the table
            ImGui::EndTable();
          }
        };

    UI::widget("FPS/Frametime",
               [&]() { draw_stats(get_timer(), *command_buffer); });

    UI::widget("Image", [&]() {
      UI::image_drop_button(texture, {128, 128});
      ImGui::SameLine();
      UI::image_button(*output_texture, {128, 128});
      ImGui::SameLine();
      UI::image_button(*output_texture_second, {128, 128});
      ImGui::SameLine();
      UI::text("Framebuffer image: ");
      UI::image_button(*framebuffer->get_image(0), {128, 128});
    });

    UI::widget("Push constant", [&]() {
      UI::text("Edit PCForMaterial Properties");
      ImGui::Separator();

      // Assuming the kernel size is between 3 and some upper limit, with a
      // step of 2 (to ensure odd values)
      int kernelInput = std::sqrt(
          pc.kernel_size); // Convert current kernel size to N for display
      if (ImGui::SliderInt("Kernel N", &kernelInput, 3, 9, "%d",
                           ImGuiSliderFlags_AlwaysClamp)) {
        if (kernelInput % 2 == 0) {
          kernelInput++; // Ensure it's always an odd number
        }
        auto [kernel, half, center] = compute_kernel_size(kernelInput);
        pc.kernel_size = kernel;
        pc.half_size = half;
        pc.center_value = center;
      }

      UI::text("Kernel Size: {}", pc.kernel_size);
      UI::text("Half Size: {}", pc.half_size);
      UI::text("Center Value: {}", pc.center_value);
    });

    ImGui::End();
  }

  for (auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  material->on_resize(new_extent);
  pipeline->on_resize(new_extent);
  shader->on_resize(new_extent);
  second_material->on_resize(new_extent);
  second_pipeline->on_resize(new_extent);
  second_shader->on_resize(new_extent);
  texture->on_resize(new_extent);
  output_texture->on_resize(new_extent);
  output_texture_second->on_resize(new_extent);

  info("{}", new_extent);
}
