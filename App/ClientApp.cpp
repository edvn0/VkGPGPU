#include "pch/vkgpgpu_pch.hpp"

#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Framebuffer.hpp"
#include "Input.hpp"
#include "Material.hpp"
#include "UI.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include "ecs/Entity.hpp"

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
  scene_renderer.set_extent(get_swapchain()->get_extent());
};

template <usize N> auto generate_points(floating K) {
  std::array<glm::mat4, N> points{};

  // Random device and generator
  std::random_device rd{};
  std::mt19937_64 gen(rd());

  // Uniform distributions for angle and z-coordinate
  std::uniform_real_distribution<float> angleDistr(0.0f, 2.0f * 3.14159265f);
  std::uniform_real_distribution<float> zDistr(-1.0f, 1.0f);

  for (size_t i = 0; i < N; ++i) {
    // Generate random angle and z-coordinate
    float angle = angleDistr(gen);
    float z = zDistr(gen);

    // Calculate the radius in the XY-plane for this z value
    float xyPlaneRadius = sqrt(1.0f - z * z);

    // Calculate the point position and scale it to a sphere of radius 3
    const auto point =
        glm::vec3(xyPlaneRadius * cos(angle), xyPlaneRadius * sin(angle), z) *
        K;
    points[i] = glm::translate(glm::mat4{1.0F}, point);
  }
  return points;
}

auto ClientApp::update_entities(floating ts) -> void {
  {
    static auto positions =
        generate_points<Config::transform_buffer_size / 2>(3.0F);

    for (const auto &pos : positions) {
      scene_renderer.submit_static_mesh(mesh.get(), pos);
    }
  }

  {
    static auto positions =
        generate_points<Config::transform_buffer_size / 2>(3.0F);

    for (const auto &pos : positions) {
      scene_renderer.submit_static_mesh(mesh.get(), pos);
    }
  }

  cube_mesh->is_shadow_caster = true;
  static auto other_positions =
      generate_points<Config::transform_buffer_size / 2>(7.0F);
  for (const auto &pos : other_positions) {
    scene_renderer.submit_static_mesh(cube_mesh.get(), pos);
  }

  cube_mesh->is_shadow_caster = true;
  static auto other_positions_again =
      generate_points<Config::transform_buffer_size / 2>(2.0F);
  for (const auto &pos : other_positions_again) {
    scene_renderer.submit_static_mesh(cube_mesh.get(), pos);
  }
}

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();
  scene->on_update(ts);
  static constexpr float radius = 8.0F;
  static glm::quat camera_orientation{1.0, 0.0, 0.0, 0.0};
  if (Input::pressed(KeyCode::KEY_D)) {
    glm::quat rotationY = glm::angleAxis(ts * 0.3F, glm::vec3(0, -1, 0));
    camera_orientation = rotationY * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_A)) {
    glm::quat rotationY = glm::angleAxis(-ts * 0.3F, glm::vec3(0, -1, 0));
    camera_orientation = rotationY * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_W)) {
    glm::quat rotationX = glm::angleAxis(ts * 0.1F, glm::vec3(1, 0, 0));
    camera_orientation = rotationX * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_S)) {
    glm::quat rotationX = glm::angleAxis(-ts * 0.1F, glm::vec3(1, 0, 0));
    camera_orientation = rotationX * camera_orientation;
  }

  camera_orientation = glm::normalize(camera_orientation);
  glm::vec3 direction = glm::rotate(camera_orientation, glm::vec3(0, 0, -1));
  camera_position = direction * radius;

  scene_renderer.begin_frame(*get_device(), frame(), camera_position);
  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

  update_entities(ts);
  {
    graphics_command_buffer->begin(frame());
    graphics(ts);
    graphics_command_buffer->end_and_submit();
  }
  compute(ts);

  {
    graphics_command_buffer->begin(frame());
    scene_drawing(ts);
    graphics_command_buffer->end_and_submit();
  }
  scene_renderer.end_frame();
  timer.end();
}

auto ClientApp::scene_drawing(floating ts) -> void {
  scene_renderer.flush(*graphics_command_buffer, frame());
}

void ClientApp::on_create() {
  for (const auto &widget : widgets) {
    widget->on_create();
  }
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
  scene_renderer.begin_renderpass(*graphics_command_buffer, *framebuffer);
  scene_renderer.bind_pipeline(*graphics_command_buffer, *graphics_pipeline);

  graphics_material->set("geometry_texture", *texture);
  scene_renderer.update_material_for_rendering(FrameIndex{frame()},
                                               *graphics_material);
  graphics_material->bind(*graphics_command_buffer, *graphics_pipeline,
                          frame());

  scene_renderer.bind_index_buffer(*graphics_command_buffer, *index_buffer);
  scene_renderer.bind_vertex_buffer(*graphics_command_buffer, *vertex_buffer);
  scene_renderer.draw(*graphics_command_buffer, {
                                                    .index_count = 3,
                                                    .instance_count = 1,
                                                });
  scene_renderer.end_renderpass(*graphics_command_buffer);
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
  scene_renderer.update_material_for_rendering(
      FrameIndex{frame()}, *material, &uniform_buffer_set, &storage_buffer_set);

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
  scene_renderer.update_material_for_rendering(
      FrameIndex{frame()}, *second_material, &uniform_buffer_set,
      &storage_buffer_set);

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
  mesh->is_shadow_caster = false;
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

  scene = make_scope<ECS::Scene>("Default");
  auto entity = scene->create_entity("Test");
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
    UI::image(scene_renderer.get_output_image(), {extent.width, extent.height});
  });

  UI::widget("Depth", [&](const auto &extent) {
    UI::image(scene_renderer.get_depth_image(), {extent.width, extent.height});
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

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  scene_renderer.set_extent(new_extent);

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
