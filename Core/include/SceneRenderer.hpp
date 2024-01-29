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
namespace std {
template <> struct hash<Core::CommandKey> {
  auto operator()(const Core::CommandKey &key) const -> Core::usize;
};

} // namespace std

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
  };
  struct ShadowUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 light_position;
    glm::vec4 light_direction;
    glm::vec4 camera_position;
  };
  struct GridUBO {
    glm::vec4 grid_colour;
    glm::vec4 plane_colour;
    glm::vec4 grid_size;
    glm::vec4 fog_colour; // alpha is fog density
  };
  struct TransformData {
    std::array<glm::mat4, Config::transform_buffer_size> transforms{};
  };
  static constexpr auto size = sizeof(TransformData);
  TransformData transform_data;
  RendererUBO renderer_ubo;
  ShadowUBO shadow_ubo;
  GridUBO grid_ubo;

  Scope<BufferSet<Buffer::Type::Uniform>> ubos;
  Scope<BufferSet<Buffer::Type::Storage>> ssbos;

  VkDescriptorPool pool;
  VkDescriptorSet active = nullptr;
  VkDescriptorSetLayout layout = nullptr;

public:
  struct DrawParameters {
    u32 index_count;
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

  void update_material_for_rendering(FrameIndex frame_index,
                                     Material &material_for_update,
                                     BufferSet<Buffer::Type::Uniform> *ubo_set,
                                     BufferSet<Buffer::Type::Storage> *sbo_set);

  void update_material_for_rendering(FrameIndex frame_index,
                                     Material &material_for_update);

  auto get_output_image() const -> const Image &;
  auto get_depth_image() const -> const Image &;

  auto set_extent(const Extent<u32> &ext) -> void { extent = ext; }

private:
  Extent<u32> extent{};

  Scope<GraphicsPipeline> geometry_pipeline;
  Scope<Shader> geometry_shader;
  Scope<Material> geometry_material;
  Scope<Framebuffer> geometry_framebuffer;

  Scope<GraphicsPipeline> shadow_pipeline;
  Scope<Shader> shadow_shader;
  Scope<Material> shadow_material;
  Scope<Framebuffer> shadow_framebuffer;

  Scope<Shader> grid_shader;
  Scope<GraphicsPipeline> grid_pipeline;
  Scope<Material> grid_material;
  Scope<Mesh> grid_mesh;

  Scope<Image> white_texture;
  Scope<Image> black_texture;
  Scope<Texture> disarray_texture;
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
};

} // namespace Core

namespace std {

inline auto
hash<Core::CommandKey>::operator()(const Core::CommandKey &key) const
    -> Core::usize {
  static constexpr std::hash<Core::u32> int_hasher;
  static constexpr std::hash<const Core::Mesh *> ptr_hasher;
  return int_hasher(key.submesh_index) ^ 0x9d4df2237 + ptr_hasher(key.mesh_ptr);
}
} // namespace std
