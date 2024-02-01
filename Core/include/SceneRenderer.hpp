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
    u32 instance_count{0};
    std::vector<glm::vec4> colours{};
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
  TransformData buffer_for_transform_data{};
  ColourData buffer_for_colour_data{};

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
  explicit SceneRenderer(const Device &dev) : device(&dev) {}

  struct DrawParameters {
    u32 index_count{};
    u32 vertex_count{0};
    u32 instance_count{1};
    u32 first_index{0};
    u32 vertex_offset{0};
    u32 first_instance{0};
  };

  auto destroy() -> void;
  auto begin_renderpass(const Framebuffer &framebuffer) -> void;

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
  auto submit_static_mesh(const Mesh *mesh, const glm::mat4 &transform = {}) {
    submit_static_mesh(mesh, transform, glm::vec4{1.0F});
  }
  auto submit_static_mesh(const Mesh *mesh, const glm::mat4 &transform,
                          const glm::vec4 &colour) -> void;
  auto end_renderpass() -> void;
  auto create(const Swapchain &swapchain) -> void;
  auto set_frame_index(FrameIndex frame_index) -> void {
    current_frame = frame_index;
  }
  auto begin_frame(const Math::Mat4 &projection, const Math::Mat4 &view)
      -> void;

  auto flush() -> void;
  auto end_frame() -> void;

  void push_constants(const GraphicsPipeline &, const Material &);
  void update_material_for_rendering(FrameIndex frame_index,
                                     Material &material_for_update,
                                     BufferSet<Buffer::Type::Uniform> *ubo_set,
                                     BufferSet<Buffer::Type::Storage> *sbo_set);

  [[nodiscard]] auto get_output_image() const -> const Image &;
  [[nodiscard]] auto get_depth_image() const -> const Image &;

  auto set_extent(const Extent<u32> &ext) -> void { extent = ext; }
  auto get_sun_position() -> auto & { return sun_position; }
  auto get_depth_factors() -> auto & { return depth_factor; }
  auto create_pool_and_layout() -> void;

  [[nodiscard]] auto get_command_buffer() const -> const auto & {
    return *command_buffer;
  }

  [[nodiscard]] auto get_compute_command_buffer() const -> const auto & {
    return *compute_command_buffer;
  }

  [[nodiscard]] static auto get_white_texture() -> const Texture & {
    return *white_texture;
  }
  [[nodiscard]] static auto get_black_texture() -> const Texture & {
    return *black_texture;
  }

private:
  const Device *device;
  Scope<CommandBuffer> command_buffer{nullptr};
  Scope<CommandBuffer> compute_command_buffer{nullptr};
  FrameIndex current_frame{0};

  Extent<u32> extent{};

  Scope<Mesh> grid_mesh;

  Scope<GraphicsPipeline> geometry_pipeline;
  Scope<Framebuffer> geometry_framebuffer;

  Scope<GraphicsPipeline> fullscreen_pipeline;
  Scope<Framebuffer> fullscreen_framebuffer;
  Scope<Material> fullscreen_material;
  Scope<Shader> fullscreen_shader;

  Scope<GraphicsPipeline> shadow_pipeline;
  Scope<Shader> shadow_shader;
  Scope<Material> shadow_material;
  Scope<Framebuffer> shadow_framebuffer;

  Scope<Shader> geometry_shader;

  Scope<Shader> grid_shader;
  Scope<GraphicsPipeline> grid_pipeline;
  Scope<Material> grid_material;

  static inline Scope<Texture> white_texture;
  static inline Scope<Texture> black_texture;
  Scope<Texture> disarray_texture;

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

  struct SubmeshTransformBuffer {
    Scope<Buffer> vertex_buffer;
    Scope<DataBuffer> transform_buffer;
  };
  std::vector<SubmeshTransformBuffer> transform_buffers;
  struct TransformVertexData {
    std::array<glm::vec4, 3> transform_rows{};
  };
  struct TransformMapData {
    std::vector<TransformVertexData> transforms;
    u32 offset = 0;
  };
  std::unordered_map<CommandKey, TransformMapData> mesh_transform_map;

  [[nodiscard]] auto is_already_bound(const GraphicsPipeline &pipeline) const
      -> bool {
    return pipeline.hash() == bound_pipeline.hash;
  }

  auto shadow_pass() -> void;
  auto grid_pass() -> void;
  auto geometry_pass() -> void;
  auto fullscreen_pass() -> void;
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
