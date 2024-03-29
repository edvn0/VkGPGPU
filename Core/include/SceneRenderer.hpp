#pragma once

#include "BufferSet.hpp"
#include "Destructors.hpp"
#include "Framebuffer.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Pipeline.hpp"
#include "Swapchain.hpp"

#include <functional>

namespace Core {
struct CommandKey;
}

template <> struct std::hash<Core::CommandKey> {
  auto operator()(const Core::CommandKey &key) const noexcept -> Core::usize;
}; // namespace std

namespace Core {

struct CommandKey {
  const Mesh *mesh_ptr{nullptr};
  u32 submesh_index{0};

  auto operator<=>(const CommandKey &) const = default;
};

class SceneRenderer {
  struct DrawCommand {
    const Mesh *mesh_ptr{};
    u32 submesh_index{0};
    std::vector<glm::mat4> transforms_and_instances{};
    Material *material{};
  };

  struct RendererUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 light_position;
    glm::vec4 light_direction;
    glm::vec4 camera_position;
  };

  struct ShadowUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec2 bias_and_default{0.005, 0.1};
  };

  struct GridUBO {
    glm::vec4 grid_colour;
    glm::vec4 plane_colour;
    glm::vec4 grid_size;
    glm::vec4 fog_colour; // alpha is fog density
  };

  struct DepthParameters {
    float value = 9.0F;
    float near = -10.0F;
    float far = 21.F;
    float bias = 0.005F;
    float default_value = 0.1F;
  };

  struct TransformData {
    std::array<glm::mat4, Config::transform_buffer_size> transforms{};
  };
  static constexpr auto size = sizeof(TransformData);
  TransformData transform_data;
  RendererUBO renderer_ubo{};
  ShadowUBO shadow_ubo{};
  GridUBO grid_ubo{};
  DepthParameters depth_factor{};

  Scope<BufferSet<Buffer::Type::Uniform>> ubos;
  Scope<BufferSet<Buffer::Type::Storage>> ssbos;

  VkDescriptorPool pool{};
  VkDescriptorSet active = nullptr;
  VkDescriptorSetLayout layout = nullptr;

public:
  struct DrawParameters {
    u32 index_count{};
    u32 instance_count{1};
    u32 first_index{0};
    u32 vertex_offset{0};
    u32 first_instance{0};
  };

  auto destroy(const Device &device) -> void;
  auto begin_renderpass(const CommandBuffer &buffer,
                        const Framebuffer &framebuffer) -> void;

  /**
   * @brief Does a full renderpass (begins + ends) which clears!
   */
  auto explicit_clear(const CommandBuffer &buffer,
                      const Framebuffer &framebuffer) -> void;

  auto draw(const CommandBuffer &buffer, const DrawParameters &params) -> void;
  auto bind_pipeline(const CommandBuffer &buffer,
                     const GraphicsPipeline &pipeline) -> void;
  auto bind_index_buffer(const CommandBuffer &buffer,
                         const Buffer &index_buffer) -> void;
  auto bind_vertex_buffer(const CommandBuffer &buffer,
                          const Buffer &vertex_buffer) -> void;
  auto submit_static_mesh(const Mesh *mesh, const glm::mat4 &transform = {})
      -> void;
  auto end_renderpass(const CommandBuffer &buffer) -> void;
  auto create(const Device &device, const Swapchain &swapchain) -> void;
  auto begin_frame(const Device &device, u32 frame,
                   const glm::vec3 &camera_position) -> void;

  auto flush(const CommandBuffer &buffer, u32 frame) -> void;
  auto end_frame() -> void;

  void push_constants(const Core::CommandBuffer &, const GraphicsPipeline &,
                      const Material &);
  void update_material_for_rendering(FrameIndex frame_index,
                                     Material &material_for_update,
                                     BufferSet<Buffer::Type::Uniform> *ubo_set,
                                     BufferSet<Buffer::Type::Storage> *sbo_set);

  [[nodiscard]] auto get_output_image() const -> const Image &;
  [[nodiscard]] auto get_depth_image() const -> const Image &;

  auto set_extent(const Extent<u32> &ext) -> void { extent = ext; }
  auto get_sun_position() -> auto & { return sun_position; }
  auto get_depth_factors() -> auto & { return depth_factor; }

  [[nodiscard]] static auto get_white_texture() -> const Texture & {
    return *white_texture;
  }
  [[nodiscard]] static auto get_black_texture() -> const Texture & {
    return *black_texture;
  }

private:
  Extent<u32> extent{};

  Scope<GraphicsPipeline> geometry_pipeline;
  Scope<Framebuffer> geometry_framebuffer;

  Scope<GraphicsPipeline> shadow_pipeline;
  Scope<Shader> shadow_shader;
  Scope<Material> shadow_material;
  Scope<Framebuffer> shadow_framebuffer;

  Scope<Shader> geometry_shader;

  Scope<Shader> grid_shader;
  Scope<GraphicsPipeline> grid_pipeline;
  Scope<Material> grid_material;
  Scope<Mesh> grid_mesh;

  static inline Scope<Texture> white_texture;
  static inline Scope<Texture> black_texture;
  Scope<Texture> disarray_texture;
  Scope<Mesh> sphere_mesh;
  Scope<Mesh> cube_mesh;

  glm::vec3 sun_position{3, -5, -3};

  struct PipelineAndHash {
    VkPipeline bound_pipeline{nullptr};
    u64 hash{0};

    auto reset() -> void {
      bound_pipeline = nullptr;
      hash = 0;
    }
  };
  PipelineAndHash bound_pipeline{};

  std::unordered_map<CommandKey, DrawCommand> draw_commands;
  std::unordered_map<CommandKey, DrawCommand> shadow_draw_commands;

  [[nodiscard]] auto is_already_bound(const GraphicsPipeline &pipeline) const
      -> bool {
    return pipeline.hash() == bound_pipeline.hash;
  }

  auto shadow_pass(const CommandBuffer &, u32) -> void;
  auto grid_pass(const CommandBuffer &, u32) -> void;
  auto geometry_pass(const CommandBuffer &, u32) -> void;
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
