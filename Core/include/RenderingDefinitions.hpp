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
  std::array<glm::vec4, 4> transform_rows{};
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
  glm::mat4 inverse_view;
  glm::mat4 inverse_projection;
  glm::mat4 inverse_view_projection;
  glm::vec4 camera_position;
  glm::vec4 light_position;
  glm::vec4 light_direction;
  glm::vec4 light_ambient_colour;
  glm::vec4 light_specular_colour;
  glm::vec2 ndc_to_view_multiplied;
  glm::vec2 ndc_to_view_added;
  glm::vec2 depth_unpacked_constants;
  glm::vec2 camera_tan_half_fov;
  glm::ivec2 tiles_count;
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
  glm::vec4 lrbt{0.0F}; // Left, right, bottom, top
  glm::vec2 nf{0.0F};   // Near, far
  glm::vec3 center{0.0F};
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

struct PointLight {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  float intensity{0.0f};
  glm::vec3 radiance{0.0f, 0.0f, 0.0f};
  float min_radius{0.001f};
  float radius{25.0f};
  float falloff{1.f};
  float source_size{0.1f};
  bool casts_shadows{true};
  std::array<char, 3> padding{0, 0, 0};
};

struct SpotLight {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  float intensity{0.0f};
  glm::vec3 direction{0.0f, 0.0f, 0.0f};
  float angle_attenuation{0.0f};
  glm::vec3 radiance{0.0f, 0.0f, 0.0f};
  float range{0.1f};
  float angle{0.0f};
  float falloff{1.0f};
  bool soft_shadows{true};
  std::array<char, 3> padding0{0, 0, 0};
  bool casts_shadows{true};
  std::array<char, 3> padding1{0, 0, 0};
};

struct PointLights {
  u32 count{0};
  glm::vec3 padding{};
  std::array<PointLight, 1000> lights{};
};

struct SpotLights {
  u32 count{0};
  glm::vec3 padding{};
  std::array<SpotLight, 1000> lights{};
};

struct SpotShadows {
  std::array<glm::mat4, 1000> shadow_matrices{};
};

struct ScreenData {
  glm::vec2 inverse_full_resolution;
  glm::vec2 full_resolution;
  glm::vec2 inverse_half_resolution;
  glm::vec2 half_resolution;
};

} // namespace Core
