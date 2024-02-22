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
  struct LineInstance {
    glm::vec4 start_position;
    glm::vec4 end_position;
    glm::vec4 colour;
  };

  auto create(const Device &, Framebuffer &, usize max_geometry_count) -> void;
  auto clear() { lines.clear(); }
  auto flush(const CommandBuffer &, FrameIndex,
             const GraphicsPipeline * = nullptr, const Material * = nullptr)
      -> void;
  auto submit(const LineInstance &vertex) -> void;
  auto update_material_for_rendering(SceneRenderer &) const -> void;

  const Device *device;
  Scope<Shader> shader;
  Scope<GraphicsPipeline> pipeline;
  Scope<Material> material;
  Scope<Buffer> instance_buffer;
  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  std::vector<LineInstance> lines;
  usize max_geometry_count{};
  static constexpr floating load_factor = 1.333;

private:
  auto recreate_buffers(bool increase_by_load_factor = false) -> void;
};

class GeometryRenderer {
public:
  explicit GeometryRenderer(Badge<SceneRenderer>, const Device &);

  auto create(Framebuffer &framebuffer, usize max_geometry_count = 100) -> void;
  auto clear() -> void;

  auto submit_aabb(const AABB &aabb, const glm::mat4 &transform,
                   const glm::vec4 &colour = Colours::white) -> void;

  auto submit_frustum(const glm::mat4 &inverse_view_projection,
                      const glm::mat4 &transform,
                      const glm::vec4 &colour = Colours::white) -> void;

  auto update_all_materials_for_rendering(SceneRenderer &) -> void;
  auto flush(const CommandBuffer &, FrameIndex,
             const GraphicsPipeline * = nullptr, const Material * = nullptr)
      -> void;

  auto get_all_materials() -> std::vector<Material *>;

private:
  const Device *device{};

  std::tuple<LineRenderer> subrenderers;
};

} // namespace Core
