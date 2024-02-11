#include "pch/vkgpgpu_pch.hpp"

#include "GeometryRenderer.hpp"

#include "Buffer.hpp"
#include "Material.hpp"
#include "Pipeline.hpp"
#include "SceneRenderer.hpp"

namespace Core {

auto LineRenderer::create(const Device &dev, Framebuffer &framebuffer,
                          usize input_max) -> void {
  device = &dev;
  max_geometry_count = input_max;
  const auto vertex_buffer_size = max_geometry_count * sizeof(LineVertex);
  const auto index_buffer_size = max_geometry_count * sizeof(u32);

  vertex_buffer =
      Buffer::construct(*device, vertex_buffer_size, Buffer::Type::Vertex);

  index_buffer =
      Buffer::construct(*device, index_buffer_size, Buffer::Type::Index);

  line_vertices.reserve(max_geometry_count);

  shader = Shader::construct(*device, FS::shader("Line.vert.spv"),
                             FS::shader("Line.frag.spv"));

  // Create pipeline
  const GraphicsPipelineConfiguration config{
      .name = "LinePipeline",
      .shader = shader.get(),
      .framebuffer = &framebuffer,
      .layout =
          {
              LayoutElement{ElementType::Float3},
              LayoutElement{ElementType::Float4},
          },
      .polygon_mode = PolygonMode::Line,
      .line_width = 5.0F,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };

  pipeline = GraphicsPipeline::construct(*device, config);

  // Create material
  material = Material::construct(*device, *shader);
}

auto LineRenderer::flush(const CommandBuffer &command_buffer, FrameIndex index)
    -> void {
  if (line_vertices.empty()) {
    return;
  }

  const auto vertex_buffer_size = line_vertices.size() * sizeof(LineVertex);
  const auto index_buffer_size = line_vertices.size() * sizeof(u32);

  vertex_buffer->write(line_vertices.data(), vertex_buffer_size);

  std::vector<u32> indices(line_vertices.size());
  for (u32 i = 0; i < line_vertices.size(); i++) {
    indices[i] = i;
  }

  index_buffer->write(indices.data(), index_buffer_size);

  // Bind pipeline
  pipeline->bind(command_buffer);
  material->bind(command_buffer, *pipeline, index);

  const auto vertex_buffers = std::array{vertex_buffer->get_buffer()};
  constexpr std::array<VkDeviceSize, 1> offsets = std::array{VkDeviceSize{0u}};
  vkCmdBindVertexBuffers(command_buffer.get_command_buffer(), 0, 1,
                         vertex_buffers.data(), offsets.data());

  const auto index_buffer_handle = index_buffer->get_buffer();
  vkCmdBindIndexBuffer(command_buffer.get_command_buffer(), index_buffer_handle,
                       0, VK_INDEX_TYPE_UINT32);

  const auto &material_pc_buffer = material->get_constant_buffer();
  vkCmdPushConstants(command_buffer.get_command_buffer(),
                     pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
                     material_pc_buffer.size(), material_pc_buffer.raw());

  vkCmdDrawIndexed(command_buffer.get_command_buffer(), line_vertices.size(), 1,
                   0, 0, 0);

  line_vertices.clear();
}

auto LineRenderer::recreate_buffers(bool increase) -> void {
  // Resize vertex and index buffer
  vertex_buffer.reset();
  index_buffer.reset();

  if (increase) {
    max_geometry_count = static_cast<u32>(
        load_factor * static_cast<floating>(max_geometry_count));
  }
  const auto vertex_buffer_size = max_geometry_count * sizeof(LineVertex);
  const auto index_buffer_size = max_geometry_count * sizeof(u32);
  vertex_buffer =
      Buffer::construct(*device, vertex_buffer_size, Buffer::Type::Vertex);

  index_buffer =
      Buffer::construct(*device, index_buffer_size, Buffer::Type::Index);
}

auto LineRenderer::submit_vertex(const LineVertex &vertex) -> void {
  if (line_vertices.size() >= max_geometry_count) {
    recreate_buffers(true);
  }

  line_vertices.push_back(vertex);
}

auto LineRenderer::update_material_for_rendering(
    SceneRenderer &scene_renderer) const -> void {
  scene_renderer.update_material_for_rendering(
      scene_renderer.get_current_index(), *material,
      scene_renderer.get_ubos().get(), scene_renderer.get_ssbos().get());
}

GeometryRenderer::GeometryRenderer(Badge<SceneRenderer>, const Device &dev)
    : device(&dev) {}

auto GeometryRenderer::create(Framebuffer &framebuffer,
                              usize max_geometry_count) -> void {
  // Create subrenderers
  std::apply(
      [&](auto &...subrenderers) {
        (subrenderers.create(*device, framebuffer, max_geometry_count), ...);
      },
      subrenderers);
}

auto GeometryRenderer::submit_aabb(const AABB &aabb,
                                   const glm::mat4 &full_transform,
                                   const glm::vec4 &colour) -> void {
  auto &line_renderer = std::get<LineRenderer>(subrenderers);
  const auto &min_aabb = aabb.min();
  const auto &max_aabb = aabb.max();

  glm::vec3 translation = glm::vec3(full_transform[3]);

  // Calculate scale
  glm::vec3 scale;
  scale.x = glm::length(glm::vec3(full_transform[0]));
  scale.y = glm::length(glm::vec3(full_transform[2]));
  scale.z = glm::length(glm::vec3(full_transform[1]));

  // Remove rotation by re-building the matrix with only translation and scale
  glm::mat4 transform = glm::mat4(1.0f); // Start with identity matrix
  transform = glm::translate(transform, translation); // Apply translation
  transform = glm::scale(transform, scale);           // Apply scale

  const std::array corners = {
      transform * glm::vec4{min_aabb.x, min_aabb.y, max_aabb.z, 1.0f},
      transform * glm::vec4{min_aabb.x, max_aabb.y, max_aabb.z, 1.0f},
      transform * glm::vec4{max_aabb.x, max_aabb.y, max_aabb.z, 1.0f},
      transform * glm::vec4{max_aabb.x, min_aabb.y, max_aabb.z, 1.0f},

      transform * glm::vec4{min_aabb.x, min_aabb.y, min_aabb.z, 1.0f},
      transform * glm::vec4{min_aabb.x, max_aabb.y, min_aabb.z, 1.0f},
      transform * glm::vec4{max_aabb.x, max_aabb.y, min_aabb.z, 1.0f},
      transform * glm::vec4{max_aabb.x, min_aabb.y, min_aabb.z, 1.0f}};

  for (u32 i = 0; i < 4; i++) {
    line_renderer.submit_vertex({.position = corners[i], .colour = colour});
    line_renderer.submit_vertex(
        {.position = corners[(i + 1) % 4], .colour = colour});
  }

  for (u32 i = 0; i < 4; i++) {
    line_renderer.submit_vertex({.position = corners[i + 4], .colour = colour});
    line_renderer.submit_vertex(
        {.position = corners[(i + 1) % 4 + 4], .colour = colour});
  }

  for (u32 i = 0; i < 4; i++) {
    line_renderer.submit_vertex({.position = corners[i], .colour = colour});
    line_renderer.submit_vertex({.position = corners[i + 4], .colour = colour});
  }
}
auto GeometryRenderer::update_all_materials_for_rendering(
    SceneRenderer &scene_renderer) -> void {
  std::apply(
      [&](auto &...subrenderers) {
        (subrenderers.update_material_for_rendering(scene_renderer), ...);
      },
      subrenderers);
}

auto GeometryRenderer::flush(const CommandBuffer &buffer, FrameIndex index)
    -> void {
  // Assume we are in an active renderpass?
  std::apply(
      [&](auto &...subrenderers) { (subrenderers.flush(buffer, index), ...); },
      subrenderers);
}
auto GeometryRenderer::get_all_materials() -> std::vector<Material *> {
  std::vector<Material *> materials;
  std::apply(
      [&](auto &...subrenderers) {
        (materials.push_back(subrenderers.material.get()), ...);
      },
      subrenderers);
  return materials;
}

auto QuadRenderer::create(const Device &dev, Framebuffer &framebuffer,
                          usize input_max) -> void {
  device = &dev;
  max_geometry_count = input_max;
  const auto vertex_buffer_size = max_geometry_count * sizeof(QuadVertex);
  const auto index_buffer_size = max_geometry_count * sizeof(u32);

  vertex_buffer =
      Buffer::construct(*device, vertex_buffer_size, Buffer::Type::Vertex);

  index_buffer =
      Buffer::construct(*device, index_buffer_size, Buffer::Type::Index);

  quad_vertices.reserve(max_geometry_count);

  shader = Shader::construct(*device, FS::shader("Quad.vert.spv"),
                             FS::shader("Quad.frag.spv"));

  // Create pipeline
  const GraphicsPipelineConfiguration config{
      .name = "QuadPipeline",
      .shader = shader.get(),
      .framebuffer = &framebuffer,
      .layout =
          {
              LayoutElement{ElementType::Float3},
              LayoutElement{ElementType::Float4},
          },
      .polygon_mode = PolygonMode::Line,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };

  pipeline = GraphicsPipeline::construct(*device, config);

  // Create material
  material = Material::construct(*device, *shader);
}

auto QuadRenderer::flush(const CommandBuffer &command_buffer, FrameIndex index)
    -> void {
  if (quad_vertices.empty()) {
    return;
  }

  const auto vertex_buffer_size = quad_vertices.size() * sizeof(QuadVertex);
  const auto index_buffer_size = quad_vertices.size() * sizeof(u32);

  vertex_buffer->write(quad_vertices.data(), vertex_buffer_size);

  std::vector<u32> indices(quad_vertices.size());
  for (u32 i = 0; i < quad_vertices.size(); i++) {
    indices[i] = i;
  }

  index_buffer->write(indices.data(), index_buffer_size);

  // Bind pipeline
  pipeline->bind(command_buffer);
  material->bind(command_buffer, *pipeline, index);

  vkCmdSetLineWidth(command_buffer.get_command_buffer(), 5.0f);

  const auto vertex_buffers = std::array{vertex_buffer->get_buffer()};
  constexpr std::array<VkDeviceSize, 1> offsets = std::array{VkDeviceSize{0u}};
  vkCmdBindVertexBuffers(command_buffer.get_command_buffer(), 0, 1,
                         vertex_buffers.data(), offsets.data());

  const auto index_buffer_handle = index_buffer->get_buffer();
  vkCmdBindIndexBuffer(command_buffer.get_command_buffer(), index_buffer_handle,
                       0, VK_INDEX_TYPE_UINT32);

  const auto &material_pc_buffer = material->get_constant_buffer();
  vkCmdPushConstants(command_buffer.get_command_buffer(),
                     pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
                     material_pc_buffer.size(), material_pc_buffer.raw());

  vkCmdDrawIndexed(command_buffer.get_command_buffer(), quad_vertices.size(), 1,
                   0, 0, 0);

  quad_vertices.clear();
}

auto QuadRenderer::submit_vertex(const QuadVertex &vertex) -> void {
  if (quad_vertices.size() >= max_geometry_count) {
    recreate_buffers(true);
  }

  quad_vertices.push_back(vertex);
}

auto QuadRenderer::update_material_for_rendering(
    SceneRenderer &scene_renderer) const -> void {
  scene_renderer.update_material_for_rendering(
      scene_renderer.get_current_index(), *material,
      scene_renderer.get_ubos().get(), scene_renderer.get_ssbos().get());
}

auto QuadRenderer::recreate_buffers(bool increase_by_load_factor) -> void {
  // Resize vertex and index buffer
  vertex_buffer.reset();
  index_buffer.reset();

  if (increase_by_load_factor) {
    max_geometry_count = static_cast<u32>(
        load_factor * static_cast<floating>(max_geometry_count));
  }
  const auto vertex_buffer_size = max_geometry_count * sizeof(QuadVertex);
  const auto index_buffer_size = max_geometry_count * sizeof(u32);
  vertex_buffer =
      Buffer::construct(*device, vertex_buffer_size, Buffer::Type::Vertex);

  index_buffer =
      Buffer::construct(*device, index_buffer_size, Buffer::Type::Index);
}

} // namespace Core
