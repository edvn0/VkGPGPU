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
auto create_or_get_write_descriptor_for(u32 frames_in_flight,
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

std::vector<glm::vec3> generate_points(auto N) {
  std::vector<glm::vec3> points{};

  // Random device and generator
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // Uniform distributions for angle and z-coordinate
  static std::uniform_real_distribution<float> angleDistr(0.0f,
                                                          2.0f * 3.14159265f);
  static std::uniform_real_distribution<float> zDistr(-1.0f, 1.0f);

  for (size_t i = 0; i < N; ++i) {
    // Generate random angle and z-coordinate
    float angle = angleDistr(gen);
    float z = zDistr(gen);

    // Calculate the radius in the XY-plane for this z value
    float xyPlaneRadius = sqrt(1.0f - z * z);

    // Calculate the point position and scale it to a sphere of radius 3
    points[i] =
        glm::vec3(xyPlaneRadius * cos(angle), xyPlaneRadius * sin(angle), z) *
        3.0f;
  }
  return points;
}

auto ClientApp::update_entities(floating ts) -> void {
  static auto positions = generate_points(10000);

  for (const auto &pos : positions) {
    auto transform = glm::translate(glm::identity<glm::mat4>(), pos);
    scene_renderer.submit_static_mesh(mesh.get(), transform);
    scene_renderer.submit_static_mesh(cube_mesh.get(), transform);
  }
}

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();
  scene_renderer.begin_frame(*get_device(), frame(), camera_position);

  update_entities(ts);

  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

  scene_drawing(ts);
  graphics(ts);
  compute(ts);

  scene_renderer.end_frame();
  timer.end();
}

auto ClientApp::scene_drawing(floating ts) -> void {}

void ClientApp::on_create() {
  for (const auto &widget : widgets)
    widget->on_create();
  perform();
}

void ClientApp::on_destroy() {
  scene_renderer.destroy(*get_device());

  for (const auto &widget : widgets) {
    widget->on_destroy();
  }
  // Destroy all fields
  command_buffer.reset();

  pipeline.reset();
  shader.reset();
  texture.reset();
  output_texture.reset();
}

auto ClientApp::graphics(floating ts) -> void {
  graphics_command_buffer->begin(frame());
  scene_renderer.begin_renderpass(*graphics_command_buffer, *framebuffer);
  scene_renderer.bind_pipeline(*graphics_command_buffer, *graphics_pipeline);

  graphics_material->set("geometry_texture", *texture);
  update_material_for_rendering(FrameIndex{frame()}, *graphics_material);
  graphics_material->bind(*graphics_command_buffer, *graphics_pipeline,
                          frame());

  scene_renderer.bind_index_buffer(*graphics_command_buffer, *index_buffer);
  scene_renderer.bind_vertex_buffer(*graphics_command_buffer, *vertex_buffer);
  scene_renderer.draw(*graphics_command_buffer, {
                                                    .index_count = 3,
                                                    .instance_count = 1,
                                                });
  scene_renderer.end_renderpass(*graphics_command_buffer, frame());
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
  material->set("input_image", *framebuffer->get_image(0));
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
          .mip_generation =
              {
                  .strategy = MipGenerationStrategy::Unused,
              },
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

  constexpr std::array vertex_data{
      -1.0F, -1.0F, 0.0F, // bottom left
      1.0F,  -1.0F, 0.0F, // bottom right
      0.0F,  1.0F,  0.0F, // top
  };
  constexpr std::array index_data{0U, 1U, 2U};
  vertex_buffer = Buffer::construct(
      *get_device(), sizeof(float) * vertex_data.size(), Buffer::Type::Vertex);
  vertex_buffer->write(vertex_data.data(), sizeof(float) * vertex_data.size());
  index_buffer = Buffer::construct(
      *get_device(), index_data.size() * sizeof(u32), Buffer::Type::Index);
  index_buffer->write(index_data.data(), index_data.size() * sizeof(u32));

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
        .blend = false,
        .attachments =
            FramebufferAttachmentSpecification{
                FramebufferTextureSpecification{
                    .format = ImageFormat::SRGB_RGBA32,
                },
                FramebufferTextureSpecification{
                    .format = ImageFormat::DEPTH32F,
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
                           .cull_mode = CullMode::Back,
                           .face_mode = FaceMode::CounterClockwise,
                       });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<127>();
    graphics_material->set("pc.kernelSize", kernel_size);
    graphics_material->set("pc.halfSize", half_size);
    graphics_material->set("pc.precomputedCenterValue", center_value);
  }

  dispatcher = make_scope<CommandDispatcher>(command_buffer.get());

  scene_renderer.create(*get_device(), *get_swapchain());

  mesh = make_scope<Mesh>();
  const std::array<Mesh::Vertex, 3> triangle_vertices{
      Mesh::Vertex{
          .pos = {0.5f, -0.5f, 0.0f},
          .uvs = {1.0f, 0.0f},
      },
      Mesh::Vertex{
          .pos = {0.5f, 0.5f, 0.0f},
          .uvs = {1.0f, 1.0f},
      },
      Mesh::Vertex{
          .pos = {-0.5f, -0.5f, 0.0f},
          .uvs = {0.0f, 0.0f},
      },
  };
  const std::array<u32, 3> triangle_indices{0, 1, 2};

  mesh->vertex_buffer = Buffer::construct(
      *get_device(), sizeof(Mesh::Vertex) * triangle_vertices.size(),
      Buffer::Type::Vertex);
  mesh->vertex_buffer->write(triangle_vertices.data(),
                             sizeof(Mesh::Vertex) * triangle_vertices.size());
  mesh->index_buffer =
      Buffer::construct(*get_device(), triangle_indices.size() * sizeof(u32),
                        Buffer::Type::Index);
  mesh->index_buffer->write(triangle_indices.data(),
                            triangle_indices.size() * sizeof(u32));
  mesh->submeshes = {
      Mesh::Submesh{
          .base_vertex = 0,
          .base_index = 0,
          .material_index = 0,
          .index_count = 3,
          .vertex_count = 3,
      },
  };
  mesh->submesh_indices.push_back(0);

  cube_mesh = Mesh::cube(*get_device());
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

void ClientApp::update_material_for_rendering(FrameIndex frame_index,
                                              Material &material_for_update) {
  update_material_for_rendering(frame_index, material_for_update, nullptr,
                                nullptr);
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
    ImGui::End();
  }

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

  /*UI::widget("Image", [&]() {
    UI::image_drop_button(texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture_second, {128, 128});
  });*/

  UI::widget("Scene", [&](const auto &extent) {
    const auto &width = framebuffer->get_width();
    const auto &height = framebuffer->get_height();
    UI::image(*framebuffer->get_image(0), {extent.width, extent.height});
  });

  UI::widget("Push constant", [&]() {
    UI::text("Edit PCForMaterial Properties");
    ImGui::Separator();

    int kernel_input = std::sqrt(pc.kernel_size);
    if (ImGui::SliderInt("Kernel N", &kernel_input, 3, 9, "%d",
                         ImGuiSliderFlags_AlwaysClamp)) {
      if (kernel_input % 2 == 0) {
        kernel_input++; // Ensure it's always an odd number
      }
      auto [kernel, half, center] = compute_kernel_size(kernel_input);
      pc.kernel_size = kernel;
      pc.half_size = half;
      pc.center_value = center;
    }

    UI::text("Kernel Size: {}", pc.kernel_size);
    UI::text("Half Size: {}", pc.half_size);
    UI::text("Center Value: {}", pc.center_value);

    ImGui::SliderFloat3("Position", &camera_position[0], -10.0f, 10.0f);
    UI::text("Current Position: x={}, y={}, z={}", camera_position.x,
             camera_position.y, camera_position.z);
  });

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

  framebuffer->on_resize(new_extent);

  info("{}", new_extent);
}

auto Mesh::cube(const Device &device) -> Scope<Mesh> {
  Scope<Mesh> output_mesh;
  output_mesh = make_scope<Mesh>();
  static constexpr const std::array<Mesh::Vertex, 8> cube_vertices{
      Mesh::Vertex{{-0.5f, -0.5f, 0.5f},
                   {0.0f, 0.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, -0.5f, 0.5f},
                   {1.0f, 0.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, 0.5f, 0.5f},
                   {1.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{-0.5f, 0.5f, 0.5f},
                   {0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},

      // Back face
      Mesh::Vertex{{-0.5f, -0.5f, -0.5f},
                   {1.0f, 0.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, -0.5f, -0.5f},
                   {0.0f, 0.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, 0.5f, -0.5f},
                   {0.0f, 1.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{-0.5f, 0.5f, -0.5f},
                   {1.0f, 1.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
  };
  static constexpr const std::array<u32, 36> cube_indices{
      0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7,
      4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3,
  };

  output_mesh->vertex_buffer =
      Buffer::construct(device, sizeof(Mesh::Vertex) * cube_vertices.size(),
                        Buffer::Type::Vertex);
  output_mesh->vertex_buffer->write(
      cube_vertices.data(), sizeof(Mesh::Vertex) * cube_vertices.size());
  output_mesh->index_buffer = Buffer::construct(
      device, cube_indices.size() * sizeof(u32), Buffer::Type::Index);
  output_mesh->index_buffer->write(cube_indices.data(),
                                   cube_indices.size() * sizeof(u32));
  output_mesh->submeshes = {
      Mesh::Submesh{
          .base_vertex = 0,
          .base_index = 0,
          .material_index = 0,
          .index_count = cube_indices.size(),
          .vertex_count = cube_vertices.size(),
      },
  };
  output_mesh->submesh_indices.push_back(0);
  return output_mesh;
}
