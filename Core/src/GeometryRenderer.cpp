#include "pch/vkgpgpu_pch.hpp"

#include "GeometryRenderer.hpp"

#include "Buffer.hpp"
#include "Material.hpp"
#include "Pipeline.hpp"
#include "SceneRenderer.hpp"

namespace Core {

GeometryRenderer::GeometryRenderer(Badge<SceneRenderer>, const Device &dev)
    : device(&dev) {}

auto GeometryRenderer::create(Framebuffer &framebuffer,
                              usize max_geometry_count) -> void {
  // Create subrenderers
  std::apply([&dev = *device, &fb = framebuffer, &geom = max_geometry_count](
                 auto &&...renderer) { (renderer.create(dev, fb, geom), ...); },
             subrenderers);
}

auto GeometryRenderer::clear() -> void {
  std::apply([](auto &&...renderer) { (renderer.clear(), ...); }, subrenderers);
}

auto GeometryRenderer::submit_aabb(const AABB &aabb,
                                   const glm::mat4 &full_transform,
                                   const glm::vec4 &colour) -> void {
  auto &line_renderer = std::get<LineRenderer>(subrenderers);
  const auto &min_aabb = aabb.min();
  const auto &max_aabb = aabb.max();

  const std::array corners = {
      full_transform * glm::vec4{min_aabb.x, min_aabb.y, max_aabb.z, 1.0f},
      full_transform * glm::vec4{min_aabb.x, max_aabb.y, max_aabb.z, 1.0f},
      full_transform * glm::vec4{max_aabb.x, max_aabb.y, max_aabb.z, 1.0f},
      full_transform * glm::vec4{max_aabb.x, min_aabb.y, max_aabb.z, 1.0f},

      full_transform * glm::vec4{min_aabb.x, min_aabb.y, min_aabb.z, 1.0f},
      full_transform * glm::vec4{min_aabb.x, max_aabb.y, min_aabb.z, 1.0f},
      full_transform * glm::vec4{max_aabb.x, max_aabb.y, min_aabb.z, 1.0f},
      full_transform * glm::vec4{max_aabb.x, min_aabb.y, min_aabb.z, 1.0f}};

  // Define the line indices for the AABB edges
  static constexpr std::array<u32, 24> indices = {
      0, 1, 1, 2, 2, 3, 3, 0, // Top face
      4, 5, 5, 6, 6, 7, 7, 4, // Bottom face
      0, 4, 1, 5, 2, 6, 3, 7  // Sides
  };

  for (u32 i = 0; i < indices.size(); i += 2) {
    line_renderer.submit({
        .start_position = corners[indices[i]],
        .end_position = corners[indices[i + 1]],
        .colour = colour,
    });
  }
}

auto GeometryRenderer::submit_frustum(const glm::mat4 &inverse_view_projection,
                                      const glm::mat4 &transform,
                                      const glm::vec4 &colour) -> void {
  auto &line_renderer = std::get<LineRenderer>(subrenderers);
  // Corners in NDC
  constexpr std::array<glm::vec3, 8> ndc_corners = {
      glm::vec3{-1.0F, -1.0F, 0.0F}, // Near Top Left (Y-axis is inverted)
      glm::vec3{1.0F, -1.0F, 0.0F},  // Near Top Right
      glm::vec3{-1.0F, 1.0F, 0.0F},  // Near Bottom Left
      glm::vec3{1.0F, 1.0F, 0.0F},   // Near Bottom Right
      glm::vec3{-1.0F, -1.0F, 1.0F}, // Far Top Left
      glm::vec3{1.0F, -1.0F, 1.0F},  // Far Top Right
      glm::vec3{-1.0F, 1.0F, 1.0F},  // Far Bottom Left
      glm::vec3{1.0F, 1.0F, 1.0F}    // Far Bottom Right
  };

  std::array<glm::vec4, 8> world_corners{};
  for (int i = 0; i < 8; ++i) {
    glm::vec4 world_corner =
        inverse_view_projection * glm::vec4(ndc_corners[i], 1.0);
    world_corners[i] = world_corner / world_corner.w; // Perspective divide
  }

  // Define lines based on world corners
  const std::array<LineRenderer::LineInstance, 12> lines = {
      LineRenderer::LineInstance{world_corners[0], world_corners[1], colour},
      {world_corners[1], world_corners[3], colour},
      {world_corners[3], world_corners[2], colour},
      {world_corners[2], world_corners[0], colour},
      {world_corners[4], world_corners[5], colour},
      {world_corners[5], world_corners[7], colour},
      {world_corners[7], world_corners[6], colour},
      {world_corners[6], world_corners[4], colour},
      {world_corners[0], world_corners[4], colour},
      {world_corners[1], world_corners[5], colour},
      {world_corners[2], world_corners[6], colour},
      {world_corners[3], world_corners[7], colour}};

  // Submit lines to the renderer
  for (auto &&line : lines) {
    line_renderer.submit(line);
  }
}

auto GeometryRenderer::update_all_materials_for_rendering(
    SceneRenderer &scene_renderer) -> void {
  std::apply(
      [&scene_rend = scene_renderer](auto &&...renderer) {
        (renderer.update_material_for_rendering(scene_rend), ...);
      },
      subrenderers);
}

auto GeometryRenderer::flush(const CommandBuffer &buffer, FrameIndex index,
                             const GraphicsPipeline *preferred_pipeline,
                             const Material *preferred_material) -> void {
  // Assume we are in an active renderpass?
  std::apply(
      [&buf = buffer, &ind = index, &pipe = preferred_pipeline,
       &mat = preferred_material](auto &&...renderer) {
        (renderer.flush(buf, ind, pipe, mat), ...);
      },
      subrenderers);
}
auto GeometryRenderer::get_all_materials() -> std::vector<Material *> {
  std::vector<Material *> materials;
  std::apply(
      [&mats = materials](auto &&...renderer) {
        (mats.push_back(renderer.material.get()), ...);
      },
      subrenderers);
  return materials;
}

auto LineRenderer::create(const Device &dev, Framebuffer &framebuffer,
                          usize input_max) -> void {
  device = &dev;
  max_geometry_count = input_max * 2;

  lines.reserve(max_geometry_count);

  shader = Shader::compile_graphics_scoped(*device, FS::shader("Line.vert"),
                                           FS::shader("Line.frag"));

  // Create pipeline
  const GraphicsPipelineConfiguration config{
      .name = "LinePipeline",
      .shader = shader.get(),
      .framebuffer = &framebuffer,
      .polygon_mode = PolygonMode::Line,
      .line_width = 5.0F,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };

  pipeline = GraphicsPipeline::construct(*device, config);

  // Create material
  material = Material::construct(*device, *shader);
  material->default_initialisation();
  recreate_buffers();
}

auto LineRenderer::flush(const CommandBuffer &command_buffer, FrameIndex index,
                         const GraphicsPipeline *shadow_pipeline,
                         const Material *shadow_material) -> void {
  if (shadow_pipeline != nullptr || shadow_material != nullptr)
    return;

  if (lines.empty()) {
    return;
  }

  // Lines never care about preferred_pipeline (i.e. shadows)

  const auto write_size = lines.size() * sizeof(LineInstance);

  instance_buffer->write(lines.data(), write_size);

  // Bind pipeline
  pipeline->bind(command_buffer);
  material->bind(command_buffer, *pipeline, index);

  vkCmdDraw(command_buffer.get_command_buffer(), 2, lines.size(), 0, 0);
}

auto LineRenderer::recreate_buffers(bool increase) -> void {
  // Resize vertex and index buffer
  if (increase) {
    max_geometry_count =
        2 * static_cast<u32>(load_factor *
                             static_cast<floating>(max_geometry_count));
  }
  const auto storage_buffer_size = max_geometry_count * sizeof(LineInstance);
  if (instance_buffer) {
    instance_buffer->resize(storage_buffer_size);
  } else {
    instance_buffer =
        Buffer::construct(*device, storage_buffer_size, Buffer::Type::Storage);
  }

  instance_buffer->set_binding(20);
  material->set("LineVertices", *instance_buffer);
}

auto LineRenderer::submit(const LineInstance &vertex) -> void {
  if (lines.size() >= max_geometry_count) {
    recreate_buffers(true);
  }

  lines.push_back(vertex);
}

auto LineRenderer::update_material_for_rendering(
    SceneRenderer &scene_renderer) const -> void {

  material->set("shadow_map", scene_renderer.get_depth_image());
  material->set("irradiance_texture",
                *scene_renderer.get_scene_environment().irradiance_texture);
  material->set("radiance_texture",
                *scene_renderer.get_scene_environment().radiance_texture);
  material->set("brdf_lookup", SceneRenderer::get_brdf_lookup_texture());
  scene_renderer.update_material_for_rendering(
      scene_renderer.get_current_index(), *material,
      scene_renderer.get_ubos().get(), scene_renderer.get_ssbos().get());
}

} // namespace Core
