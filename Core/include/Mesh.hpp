#pragma once

#include "AABB.hpp"
#include "Buffer.hpp"
#include "Filesystem.hpp"
#include "Material.hpp"
#include "Types.hpp"

#include <glm/glm.hpp>
#include <span>
#include <vector>

#include "core/Forward.hpp"

struct aiMaterial;
struct aiString;

namespace Core {

struct ImporterImpl;

struct Index {
  u32 zero{0};
  u32 one{0};
  u32 two{0};
};

struct Submesh {
  u32 base_vertex{0};
  u32 base_index{0};
  u32 material_index{0};
  u32 index_count{0};
  u32 vertex_count{0};

  glm::mat4 transform{1.0F};
  glm::mat4 local_transform{1.0F};
  AABB bounding_box;
};

struct Vertex {
  glm::vec3 pos;
  glm::vec2 uvs;
  glm::vec4 colour{1.0F};
  glm::vec3 normals{0.F};
  glm::vec3 tangents{0.F};
  glm::vec3 bitangents{0.F};
};

class Mesh {
public:
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
  [[nodiscard]] auto get_aabb() const -> const AABB & { return bounding_box; }

  [[nodiscard]] constexpr auto casts_shadows() const -> bool {
    return is_shadow_caster;
  }
  [[nodiscard]] auto is_shadow_casting(bool casts) { is_shadow_caster = casts; }

  [[nodiscard]] auto get_file_path() const -> const FS::Path & {
    return file_path;
  }

  static auto get_cube() -> const Ref<Mesh> &;

  static auto import_from(const Device &device, const FS::Path &file_path)
      -> Scope<Mesh>;
  static auto reference_import_from(const Device &device,
                                    const FS::Path &file_path)
      -> const Ref<Mesh> &;

  static auto clear_cache() -> void { mesh_cache.clear(); }

private:
  Mesh(const Device &device, const FS::Path &);
  const Device *device;
  const FS::Path file_path;

  std::vector<Vertex> vertices;
  std::vector<Index> indices;

  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  std::vector<Ref<Material>> materials;
  std::unordered_map<u32, std::vector<u32>> material_to_submesh_indices;
  std::unordered_map<u32, u32> submesh_to_material_index;
  std::vector<Submesh> submeshes;
  std::vector<u32> submesh_indices;

  Scope<Shader> default_shader;
  std::unordered_map<std::string, Scope<Texture>> mesh_owned_textures;

  AABB bounding_box;

  [[nodiscard]] auto
  read_texture_from_file_path(const std::string &texture_path) const
      -> Scope<Texture>;
  void handle_normal_map(const aiMaterial *ai_material,
                         Material &submesh_material, aiString ai_tex_path);
  void handle_roughness_map(const aiMaterial *ai_material,
                            Material &submesh_material, aiString ai_tex_path,
                            float roughness);
  void handle_metalness_map(const aiMaterial *ai_material,
                            Material &submesh_material, float metalness);
  void handle_albedo_map(const aiMaterial *ai_material,
                         Material &submesh_material, aiString ai_tex_path);

  struct Deleter {
    auto operator()(ImporterImpl *pimpl) const -> void;
  };
  Scope<ImporterImpl, Deleter> importer;

  bool is_shadow_caster{true};

  static inline std::unordered_map<std::string, Ref<Mesh>> mesh_cache{};
};

} // namespace Core
