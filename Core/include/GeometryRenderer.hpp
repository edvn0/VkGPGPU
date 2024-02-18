#pragma once

#include "AABB.hpp"
#include "BufferSet.hpp"
#include "Colours.hpp"
#include "Device.hpp"
#include "Framebuffer.hpp"
#include "Pipeline.hpp"
#include "Types.hpp"

#include <glm/glm.hpp>

namespace Core {

class SceneRenderer;

struct LineRenderer {
  struct LineVertex {
    glm::vec4 position;
    glm::vec4 colour;
  };

  auto create(const Device &, Framebuffer &, usize max_geometry_count) -> void;
  auto flush(const CommandBuffer &, FrameIndex) -> void;
  auto submit_vertex(const LineVertex &vertex) -> void;
  auto update_material_for_rendering(SceneRenderer &) const -> void;

  const Device *device;
  Scope<Shader> shader;
  Scope<GraphicsPipeline> pipeline;
  Scope<Material> material;
  Scope<Buffer> instance_buffer;
  std::vector<LineVertex> line_vertices;
  usize max_geometry_count{};
  static constexpr floating load_factor = 1.333;

private:
  auto recreate_buffers(bool increase_by_load_factor = false) -> void;
};

struct CubeRenderer {
  struct CubeInstance {
    const glm::mat4 transform = glm::mat4{1.0F};
    const glm::vec4 colour = Colours::white;
    // Three dummy values
    const glm::vec4 side_length = glm::vec4{1.0F};
  };

  auto create(const Device &, Framebuffer &, usize max_geometry_count) -> void;
  auto flush(const CommandBuffer &, FrameIndex) -> void;
  auto submit(Core::floating side_length, const glm::mat4 &transform,
              const glm::vec4 &colour = Colours::white) -> void;
  auto update_material_for_rendering(SceneRenderer &) const -> void;

  const Device *device;
  Scope<Shader> shader;
  Scope<GraphicsPipeline> pipeline;
  Scope<Material> material;
  Scope<Buffer> instance_buffer;
  std::vector<CubeInstance> instance_data;
  usize max_geometry_count{};
  static constexpr floating load_factor = 1.333;

private:
  auto recreate_buffers(bool increase_by_load_factor = false) -> void;
};

struct QuadRenderer {
  struct QuadVertex {
    glm::vec3 position;
    glm::vec4 colour;
  };

  auto create(const Device &, Framebuffer &, usize max_geometry_count) -> void;
  auto flush(const CommandBuffer &, FrameIndex) -> void;
  auto submit_vertex(const QuadVertex &vertex) -> void;
  auto update_material_for_rendering(SceneRenderer &) const -> void;

  const Device *device;
  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  Scope<Shader> shader;
  Scope<GraphicsPipeline> pipeline;
  Scope<Material> material;
  std::vector<QuadVertex> quad_vertices;
  usize max_geometry_count{};
  static constexpr floating load_factor = 1.333;

private:
  auto recreate_buffers(bool increase_by_load_factor = false) -> void;
};

class GeometryRenderer {
public:
  explicit GeometryRenderer(Badge<SceneRenderer>, const Device &);

  auto create(Framebuffer &framebuffer, usize max_geometry_count = 100) -> void;

  auto submit_aabb(const AABB &aabb, const glm::mat4 &transform,
                   const glm::vec4 &colour = Colours::white) -> void;

  auto submit_cube(Core::floating side_length, const glm::mat4 &transform,
                   const glm::vec4 &colour = Colours::white) -> void;

  auto update_all_materials_for_rendering(SceneRenderer &) -> void;
  auto flush(const CommandBuffer &, FrameIndex) -> void;

  auto get_all_materials() -> std::vector<Material *>;

private:
  const Device *device{};

  std::tuple<LineRenderer, QuadRenderer, CubeRenderer> subrenderers;
};

} // namespace Core
