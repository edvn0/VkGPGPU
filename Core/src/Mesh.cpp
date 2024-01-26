#include "pch/vkgpgpu_pch.hpp"

#include "Mesh.hpp"

#include "Material.hpp"

namespace Core {

auto Mesh::get_materials_span() const -> std::span<Material *> {
  std::vector<Material *> raw_pointers;
  raw_pointers.reserve(materials.size());
  for (auto &uniquePtr : materials) {
    raw_pointers.push_back(uniquePtr.get());
  }

  return std::span<Material *>(raw_pointers.data(), raw_pointers.size());
}

auto Mesh::get_material(u32 index) const -> Material * {
  if (materials.size() < index && index > 0) {
    return materials.at(index).get();
  }
  return nullptr;
}

auto Mesh::cube(const Device &device) -> Scope<Mesh> {
  Scope<Mesh> output_mesh;
  output_mesh = make_scope<Mesh>();
  static constexpr const std::array<Mesh::Vertex, 8> cube_vertices{
      Mesh::Vertex{{-0.5f, -0.5f, 0.5f},
                   {0.0f, 0.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, -0.5f, 0.5f},
                   {1.0f, 0.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, 0.5f, 0.5f},
                   {1.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{-0.5f, 0.5f, 0.5f},
                   {0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, 1.0f},
                   {1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},

      // Back face
      Mesh::Vertex{{-0.5f, -0.5f, -0.5f},
                   {1.0f, 0.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, -0.5f, -0.5f},
                   {0.0f, 0.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{0.5f, 0.5f, -0.5f},
                   {0.0f, 1.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
      Mesh::Vertex{{-0.5f, 0.5f, -0.5f},
                   {1.0f, 1.0f},
                   {1.0f, 1.0f, 0.0f, 1.0f},
                   {0.0f, 0.0f, -1.0f},
                   {-1.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f}},
  };
  static constexpr const std::array<u32, 36> cube_indices{
      0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7,
      4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3,
  };

  output_mesh->vertex_buffer =
      Buffer::construct(device, sizeof(Mesh::Vertex) * cube_vertices.size(),
                        Buffer::Type::Vertex);
  output_mesh->vertex_buffer->write(
      cube_vertices.data(), sizeof(Mesh::Vertex) * cube_vertices.size());
  output_mesh->index_buffer = Buffer::construct(
      device, cube_indices.size() * sizeof(u32), Buffer::Type::Index);
  output_mesh->index_buffer->write(cube_indices.data(),
                                   cube_indices.size() * sizeof(u32));
  output_mesh->submeshes = {
      Mesh::Submesh{
          .base_vertex = 0,
          .base_index = 0,
          .material_index = 0,
          .index_count = cube_indices.size(),
          .vertex_count = cube_vertices.size(),
      },
  };
  output_mesh->submesh_indices.push_back(0);
  return output_mesh;
}

} // namespace Core
