#pragma once

#include "BufferSet.hpp"
#include "Destructors.hpp"
#include "Framebuffer.hpp"
#include "GeometryRenderer.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Pipeline.hpp"
#include "RenderingDefinitions.hpp"
#include "SceneResources.hpp"
#include "Swapchain.hpp"
#include "TextureCube.hpp"

#include <functional>

#include "ecs/Scene.hpp"

namespace Core {
struct CommandKey;
}

template <> struct std::hash<Core::CommandKey> {
  auto operator()(const Core::CommandKey &key) const noexcept -> Core::usize;
}; // namespace std

namespace Core {

using ShaderCache = Container::StringLikeMap<Scope<Shader>>;

struct CommandKey {
  const Mesh *mesh_ptr{nullptr};
  u32 submesh_index{0};

  auto operator<=>(const CommandKey &) const = default;
};

class SceneRenderer {
public:
  explicit SceneRenderer(const Device &dev)
      : device(&dev), geometry_renderer({}, *device) {}

  auto destroy() -> void;
  auto begin_renderpass(const Framebuffer &framebuffer) -> void;
  auto on_resize(const Extent<u32> &extent) -> void;

  /**
   * @brief Does a full renderpass (begins + ends) which clears!
   */
  auto explicit_clear(const Framebuffer &framebuffer) -> void;

  auto draw(const DrawParameters &params) const -> void;
  auto draw_vertices(const DrawParameters &params) const -> void;
  auto bind_pipeline(const GraphicsPipeline &pipeline) -> void;
  auto bind_index_buffer(const Buffer &index_buffer) const -> void;
  auto bind_vertex_buffer(const Buffer &vertex_buffer) const -> void;
  auto bind_vertex_buffer(const Buffer &vertex_buffer, u32 offset,
                          u32 index) const -> void;
  auto submit_aabb(const AABB &aabb,
                   const glm::mat4 &transform = glm::mat4{1.0F}) -> void;
  auto submit_cube(const glm::mat4 &transform = glm::mat4{1.0F},
                   const glm::vec4 &colour = Colours::white) -> void;
  auto submit_frustum(const glm::mat4 &inverse_view_projection,
                      const glm::mat4 &transform = glm::mat4{1.0F}) -> void;
  auto submit_static_mesh(const Mesh *mesh, const glm::mat4 &transform = {},
                          const glm::vec4 &colour = Colours::white) -> void;
  auto end_renderpass() -> void;
  auto create(const Swapchain &swapchain) -> void;
  auto begin_frame(const Math::Mat4 &projection, const Math::Mat4 &view)
      -> void;

  auto begin_scene(const ECS::Scene &, FrameIndex frame_index) -> void;

  auto flush() -> void;
  auto end_frame() -> void;

  void push_constants(const GraphicsPipeline &, const Material &);
  void push_constants(const ComputePipeline &, const Material &);
  void update_material_for_rendering(FrameIndex frame_index,
                                     Material &material_for_update,
                                     BufferSet<Buffer::Type::Uniform> *ubo_set,
                                     BufferSet<Buffer::Type::Storage> *sbo_set);

  [[nodiscard]] auto get_output_image() const -> const Image &;
  [[nodiscard]] auto get_depth_image() const -> const Image &;

  auto get_sun_position() -> auto & { return sun_position; }
  auto get_depth_factors() -> auto & { return depth_factor; }
  auto get_grid_configuration() -> auto & { return grid_ubo; }
  auto get_renderer_configuration() -> auto & { return renderer_ubo; }
  auto get_shadow_configuration() -> auto & { return shadow_ubo; }
  auto get_scene_environment() -> auto & { return scene_environment; }
  auto get_bloom_configuration() -> auto & { return bloom_settings; }

  [[nodiscard]] static auto get_white_texture() -> const Texture & {
    return *white_texture;
  }
  [[nodiscard]] static auto get_black_texture() -> const Texture & {
    return *black_texture;
  }
  [[nodiscard]] static auto get_brdf_lookup_texture() -> const Texture & {
    return *brdf_lookup_texture;
  }
  [[nodiscard]] auto get_current_index() const -> FrameIndex {
    return current_frame;
  }
  [[nodiscard]] auto get_ubos() -> Scope<BufferSet<Buffer::Type::Uniform>> & {
    return ubos;
  }
  [[nodiscard]] auto get_ssbos() -> Scope<BufferSet<Buffer::Type::Storage>> & {
    return ssbos;
  }
  [[nodiscard]] auto get_extent() const -> const auto & { return extent; }
  [[nodiscard]] auto get_graphics_command_buffer() -> const auto & {
    return *command_buffer;
  }
  [[nodiscard]] auto get_compute_command_buffer() -> const auto & {
    return *compute_command_buffer;
  }
  [[nodiscard]] auto get_gpu_execution_times() -> const auto & {
    return gpu_time_queries;
  }

  auto set_opacity(std::floating_point auto new_opacity) {
    opacity = new_opacity;
  }

  static auto begin_gpu_debug_frame_marker(const CommandBuffer &,
                                           std::string_view) -> void;
  static auto end_gpu_debug_frame_marker(const CommandBuffer &,
                                         std::string_view) -> void;

  [[nodiscard]] auto get_shader_cache() -> auto & { return shader_cache; }

private:
  auto create_pool_and_layout() -> void;

  const Device *device;
  Scope<CommandBuffer> command_buffer{nullptr};
  Scope<CommandBuffer> compute_command_buffer{nullptr};
  FrameIndex current_frame{0};

  GeometryRenderer geometry_renderer;

  Extent<u32> extent{};
  Extent<float> inverse_extent{};

  Scope<GraphicsPipeline> geometry_pipeline;
  Scope<GraphicsPipeline> wireframed_geometry_pipeline;
  Scope<Framebuffer> geometry_framebuffer;

  Scope<GraphicsPipeline> fullscreen_pipeline;
  Scope<Framebuffer> fullscreen_framebuffer;
  Scope<Material> fullscreen_material;

  struct SceneInfo {
    ECS::LightEnvironment light_environment;
  };
  SceneInfo scene_data;

  floating opacity{1.0F};

  struct BloomSettings {
    bool enabled = true;
    floating threshold = 1.0F;
    floating knee = 0.1F;
    floating upsample_scale = 1.0F;
    floating intensity = 1.0F;
    floating dirt_intensity = 0.0F;
    floating opacity = 1.0F;
  };
  u32 bloom_workgroup_size = 4;
  BloomSettings bloom_settings{};
  Scope<ComputePipeline> bloom_pipeline;
  std::array<Scope<Texture>, 3> bloom_textures{};
  Scope<Material> bloom_material;

  glm::uvec3 light_culling_workgroup_size = {16, 1, 1};
  Scope<ComputePipeline> light_culling_pipeline;
  Scope<Material> light_culling_material;

  Scope<GraphicsPipeline> skybox_pipeline;
  Scope<Material> skybox_material;

  Scope<GraphicsPipeline> shadow_pipeline;
  Scope<Material> shadow_material;
  Scope<Framebuffer> shadow_framebuffer;

  Scope<GraphicsPipeline> spot_shadow_pipeline;
  Scope<Material> spot_shadow_material;
  Scope<Framebuffer> spot_shadow_framebuffer;

  Scope<GraphicsPipeline> predepth_pipeline;
  Scope<Material> predepth_material;
  Scope<Framebuffer> predepth_framebuffer;

  Scope<GraphicsPipeline> grid_pipeline;
  Scope<Material> grid_material;

  static inline Scope<Texture> white_texture;
  static inline Scope<Texture> black_texture;
  static inline Scope<Texture> brdf_lookup_texture;

  glm::vec3 sun_position{3, -5, -3};

  PipelineAndHash bound_pipeline{};

  std::unordered_map<CommandKey, DrawCommand> draw_commands;
  std::unordered_map<CommandKey, DrawCommand> shadow_draw_commands;

  std::vector<SubmeshTransformBuffer> transform_buffers;
  std::unordered_map<CommandKey, TransformMapData> mesh_transform_map;

  TransformData buffer_for_transform_data{};

  RendererUBO renderer_ubo{};
  ShadowUBO shadow_ubo{};
  GridUBO grid_ubo{};
  DepthParameters depth_factor{};
  SpotLights spot_light_ubo{};
  PointLights point_light_ubo{};
  SpotShadows spot_shadows_ubo{};
  ScreenData screen_data_ubo{};

  struct GPUTimeQueries {
    u32 predepth_query{0};
    u32 directional_shadow_pass_query{0};
    u32 spot_shadow_pass_query{0};
    u32 light_culling_pass_query{0};
    u32 geometry_pass_query{0};
    u32 bloom_compute_pass_query{0};
    u32 composite_pass_query{0};
  };
  GPUTimeQueries gpu_time_queries;

  SceneEnvironment scene_environment{};

  Scope<BufferSet<Buffer::Type::Uniform>> ubos;
  Scope<BufferSet<Buffer::Type::Storage>> ssbos;

  VkDescriptorPool pool{};
  VkDescriptorSet active = nullptr;
  VkDescriptorSetLayout layout = nullptr;

  ShaderCache shader_cache{};

  auto create_preetham_sky(float turbidity, float azimuth, float inclination)
      -> Ref<TextureCube>;

  auto on_flush() -> void;

  auto predepth_pass() -> void;
  auto shadow_pass() -> void;
  auto spot_shadow_pass() -> void;
  auto grid_pass() -> void;
  auto light_culling_pass() -> void;
  auto geometry_pass() -> void;
  auto bloom_pass() -> void;
  auto debug_pass() -> void;
  auto environment_pass() -> void;
  auto fullscreen_pass() -> void;

  auto set_frame_index(FrameIndex frame_index) -> void {
    current_frame = frame_index;
  }

  static inline std::stack<std::string> debug_marker_stack{};
};

} // namespace Core

namespace std {

inline auto
hash<Core::CommandKey>::operator()(const Core::CommandKey &key) const noexcept
    -> Core::usize {
  return std::hash<const Core::Mesh *>()(key.mesh_ptr) ^
         std::hash<Core::u32>()(key.submesh_index);
}

} // namespace std
