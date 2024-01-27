#pragma once

#include "Buffer.hpp"
#include "Types.hpp"

#include <glm/glm.hpp>
#include <span>
#include <vector>

#include "core/Forward.hpp"

namespace Core {

struct Mesh {
  struct Vertex {
    glm::vec3 pos;
    glm::vec2 uvs;
    glm::vec4 colour{1.0F};
    glm::vec3 normals{0.F};
    glm::vec3 tangents{0.F};
    glm::vec3 bitangents{0.F};
  };

  struct Submesh {
    u32 base_vertex;
    u32 base_index;
    u32 material_index;
    u32 index_count;
    u32 vertex_count;

    // glm::mat4 transform{ 1.0f };
    // glm::mat4 local_transform{ 1.0f };
    // AABB bounding_box;
  };
  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  std::vector<Scope<Material>> materials;

  std::vector<Submesh> submeshes;
  std::vector<u32> submesh_indices;
  [[nodiscard]] auto get_submeshes() const -> const auto & {
    return submesh_indices;
  }
  [[nodiscard]] auto get_submesh(u32 index) const -> const auto & {
    return submeshes.at(index);
  }
  [[nodiscard]] auto get_materials_span() const -> std::span<Material *>;
  [[nodiscard]] auto get_material(u32 index) const -> Material *;
  [[nodiscard]] auto has_material(u32 index) const -> bool {
    return materials.size() > index;
  }

  [[nodiscard]] auto get_vertex_buffer() const -> const auto & {
    return vertex_buffer;
  }
  [[nodiscard]] auto get_index_buffer() const -> const auto & {
    return index_buffer;
  }

  static auto cube(const Device &) -> Scope<Mesh>;

  constexpr auto casts_shadows() const -> bool { return is_shadow_caster; }
  bool is_shadow_caster{true};
};

} // namespace Core
