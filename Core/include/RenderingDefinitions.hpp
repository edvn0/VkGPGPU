#pragma once

#include "Types.hpp"

#include <array>
#include <glm/glm.hpp>
#include <vector>

#include "core/Forward.hpp"

namespace Core {

struct PipelineAndHash {
  VkPipeline bound_pipeline{nullptr};
  u64 hash{0};

  auto reset() -> void {
    bound_pipeline = nullptr;
    hash = 0;
  }
};
struct SubmeshTransformBuffer {
  Scope<Buffer> vertex_buffer;
  Scope<DataBuffer> transform_buffer;
};
struct TransformVertexData {
  std::array<glm::vec4, 3> transform_rows{};
};
struct TransformMapData {
  std::vector<TransformVertexData> transforms;
  u32 offset = 0;
};

struct DrawCommand {
  const Mesh *mesh_ptr{};
  u32 submesh_index{0};
  u32 instance_count{0};
  Material *material{};
};

struct RendererUBO {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 view_projection;
  glm::vec4 light_position;
  glm::vec4 light_direction;
  glm::vec4 camera_position;
  glm::vec4 light_colour;
};

struct ShadowUBO {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 view_projection;
  glm::vec2 bias_and_default{0.005, 0.1};
};

struct GridUBO {
  glm::vec4 grid_colour{0.5F, 0.5F, 0.5F, 1.0F};
  glm::vec4 plane_colour{0.5F, 0.5F, 0.5F, 1.0F};
  glm::vec4 grid_size{0.2F, 0.2F, 0.1F,
                      60.0F}; // z and w are for the near and far planes
  glm::vec4 fog_colour{0.5F, 0.5F, 0.5F, 1.0F};
};

struct DepthParameters {
  float value = 9.0F;
  float near = -10.0F;
  float far = 21.F;
  float bias = 0.005F;
  float default_value = 0.1F;
};

struct TransformData {
  std::vector<glm::mat4> transforms{};

  [[nodiscard]] auto size() const -> usize {
    return transforms.size() * sizeof(glm::mat4);
  }
};
struct ColourData {
  std::vector<glm::vec4> colours{};

  [[nodiscard]] auto size() const -> usize {
    return colours.size() * sizeof(glm::vec4);
  }
};
struct DrawParameters {
  u32 index_count{};
  u32 vertex_count{0};
  u32 instance_count{1};
  u32 first_index{0};
  u32 vertex_offset{0};
  u32 first_instance{0};
};

} // namespace Core
