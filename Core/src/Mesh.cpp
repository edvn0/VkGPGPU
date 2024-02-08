#include "pch/vkgpgpu_pch.hpp"

#include "Mesh.hpp"

#include "Formatters.hpp"
#include "Logger.hpp"
#include "Material.hpp"
#include "SceneRenderer.hpp"

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <assimp/types.h>
#include <stb_image.h>

namespace Core {

auto Mesh::get_materials_span() const -> std::span<Material *> {
  std::vector<Material *> raw_pointers;
  raw_pointers.reserve(materials.size());
  for (auto &uniquePtr : materials) {
    raw_pointers.push_back(uniquePtr.get());
  }

  return {raw_pointers.data(), raw_pointers.size()};
}

auto Mesh::get_material(u32 index) const -> Material * {
  // Check if the submesh index has a corresponding material index
  if (const auto it = submesh_to_material_index.find(index);
      it != submesh_to_material_index.end()) {
    if (const u32 material_index = it->second;
        material_index < materials.size()) {
      return materials.at(material_index).get();
    }
  }
  return nullptr;
}

struct InMemoryImageLoader {
  explicit InMemoryImageLoader(const void *input_data,
                               std::integral auto input_width,
                               std::integral auto input_height,
                               DataBuffer &input_buffer)
      : buffer(input_buffer) {
    if (input_data == nullptr) {
      error("InMemoryImageLoader", "Could not load embedded texture.");
      return;
    }

    constexpr std::int32_t requested_channels = STBI_rgb_alpha;
    const auto size = input_height > 0
                          ? input_width * input_height * requested_channels
                          : input_width;

    stbi_set_flip_vertically_on_load(true);
    auto *loaded_image =
        stbi_load_from_memory(static_cast<const u8 *>(input_data), size, &width,
                              &height, &channels, requested_channels);

    const auto span = std::span{
        loaded_image,
        static_cast<std::size_t>(width * height * channels),
    };
    const auto padded_buffer = pad_with_alpha(span);
    const auto loaded_size = width * height * requested_channels;

    const DataBuffer data_buffer{padded_buffer.data(), loaded_size};
    stbi_image_free(loaded_image);

    input_buffer.copy_from(data_buffer);
  }

  std::int32_t width{};
  std::int32_t height{};
  std::int32_t channels{};
  DataBuffer &buffer;

private:
  auto pad_with_alpha(const auto data) const -> std::vector<u8> {
    if (channels == 4) {
      // Already has an alpha channel
      return {data.begin(), data.end()};
    }

    std::vector<u8> new_data(width * height * 4); // 4 channels for RGBA

    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        const int new_index = (y * width + x) * 4;
        const int old_index = (y * width + x) * channels;

        for (int j = 0; j < channels; ++j) {
          new_data[new_index + j] = data[old_index + j];
        }

        new_data[new_index + 3] = 255;
      }
    }

    return new_data;
  }
};

struct ImporterImpl {
  Scope<Assimp::Importer> importer{nullptr};
  std::unordered_map<aiNode *, std::vector<u32>> node_map;
  const aiScene *scene;
};

auto Mesh::Deleter::operator()(ImporterImpl *impl) -> void { delete impl; }

namespace {
constexpr auto to_mat4_from_assimp(const aiMatrix4x4 &matrix) -> glm::mat4 {
  glm::mat4 result;
  result[0][0] = matrix.a1;
  result[1][0] = matrix.a2;
  result[2][0] = matrix.a3;
  result[3][0] = matrix.a4;
  result[0][1] = matrix.b1;
  result[1][1] = matrix.b2;
  result[2][1] = matrix.b3;
  result[3][1] = matrix.b4;
  result[0][2] = matrix.c1;
  result[1][2] = matrix.c2;
  result[2][2] = matrix.c3;
  result[3][2] = matrix.c4;
  result[0][3] = matrix.d1;
  result[1][3] = matrix.d2;
  result[2][3] = matrix.d3;
  result[3][3] = matrix.d4;
  return result;
}
} // namespace

auto traverse_nodes(auto &submeshes, auto &importer, aiNode *node,
                    const glm::mat4 &parent_transform = glm::mat4{1.0F},
                    u32 level = 0) -> void {
  if (node == nullptr) {
    return;
  }

  const glm::mat4 local_transform =
      glm::mat4{1.0F}; // to_mat4_from_assimp(node->mTransformation);
  const glm::mat4 transform = parent_transform * local_transform;
  const std::span node_meshes{node->mMeshes, node->mNumMeshes};

  importer->node_map[node].resize(node_meshes.size());
  for (auto i = 0; i < node_meshes.size(); i++) {
    const u32 mesh = node_meshes[i];
    auto &submesh = submeshes[mesh];
    submesh.transform = transform;
    submesh.local_transform = local_transform;
    importer->node_map[node][i] = mesh;
  }

  for (std::span children{node->mChildren, node->mNumChildren};
       auto &child : children) {
    traverse_nodes(submeshes, importer, child, transform, level + 1);
  }
}

static constexpr u32 mesh_import_flags =
    aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_CalcTangentSpace |
    aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_FlipUVs;

auto Mesh::import_from(const Device &device, const FS::Path &file_path)
    -> Scope<Mesh> {
  return Scope<Mesh>(new Mesh{device, file_path});
}
auto Mesh::reference_import_from(const Device &device,
                                 const FS::Path &file_path) -> Ref<Mesh> {
  const std::string key = file_path.string();
  if (mesh_cache.contains(key)) {
    return mesh_cache.at(key);
  }

  auto mesh = Ref<Mesh>(new Mesh{device, key});
  mesh_cache.try_emplace(key, mesh);

  return mesh_cache.at(key);
}

Mesh::Mesh(const Device &dev, const FS::Path &path)
    : device(&dev), file_path(path) {
  importer = make_scope<ImporterImpl, Mesh::Deleter>();
  importer->importer = make_scope<Assimp::Importer>();

  default_shader = Shader::construct(*device, FS::shader("Basic.vert.spv"),
                                     FS::shader("Basic.frag.spv"));

  const aiScene *loaded_scene =
      importer->importer->ReadFile(file_path.string(), mesh_import_flags);
  if (loaded_scene == nullptr) {
    error("Mesh", "Failed to load mesh file: {0}", file_path.string());
    return;
  }

  importer->scene = loaded_scene;

  if (!importer->scene->HasMeshes()) {
    return;
  }

  u32 vertex_count = 0;
  u32 index_count = 0;

  const auto num_meshes = importer->scene->mNumMeshes;

  submeshes.reserve(num_meshes);
  for (u32 submesh_index = 0; submesh_index < num_meshes; submesh_index++) {
    aiMesh const *mesh = importer->scene->mMeshes[submesh_index];

    Submesh &submesh = submeshes.emplace_back();
    submesh.base_vertex = vertex_count;
    submesh.base_index = index_count;
    submesh.material_index = mesh->mMaterialIndex;
    submesh.vertex_count = mesh->mNumVertices;
    submesh.index_count = mesh->mNumFaces * 3;
    submesh_indices.push_back(submesh_index);
    material_to_submesh_indices[submesh.material_index].push_back(
        submesh_index);
    submesh_to_material_index[submesh_index] = submesh.material_index;

    vertex_count += mesh->mNumVertices;
    index_count += submesh.index_count;

    ensure(mesh->HasPositions(), "Meshes require positions.");
    ensure(mesh->HasNormals(), "Meshes require normals.");

    // Vertices
    auto &submesh_aabb = submesh.bounding_box;
    for (std::size_t i = 0; i < mesh->mNumVertices; i++) {
      Vertex vertex{};
      vertex.pos = {mesh->mVertices[i].x, mesh->mVertices[i].y,
                    mesh->mVertices[i].z};
      vertex.normals = {mesh->mNormals[i].x, mesh->mNormals[i].y,
                        mesh->mNormals[i].z};
      submesh_aabb.update(vertex.pos);

      if (mesh->HasTangentsAndBitangents()) {
        vertex.tangents = {mesh->mTangents[i].x, mesh->mTangents[i].y,
                           mesh->mTangents[i].z};
        vertex.bitangents = {mesh->mBitangents[i].x, mesh->mBitangents[i].y,
                             mesh->mBitangents[i].z};
      }

      if (mesh->HasTextureCoords(0)) {
        vertex.uvs = {mesh->mTextureCoords[0][i].x,
                      mesh->mTextureCoords[0][i].y};
      }

      vertices.push_back(vertex);
    }

    // Indices
    for (std::size_t i = 0; i < mesh->mNumFaces; i++) {
      ensure(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices.");
      const Index index = {
          mesh->mFaces[i].mIndices[0],
          mesh->mFaces[i].mIndices[1],
          mesh->mFaces[i].mIndices[2],
      };
      indices.push_back(index);
    }
  }

  vertex_buffer = Buffer::construct(*device, vertices.size() * sizeof(Vertex),
                                    Buffer::Type::Vertex, 0);
  vertex_buffer->write(std::span{vertices});
  index_buffer = Buffer::construct(*device, indices.size() * sizeof(Index),
                                   Buffer::Type::Index, 0);
  index_buffer->write(std::span{indices});

  traverse_nodes(submeshes, importer, importer->scene->mRootNode);

  for (const auto &submesh : submeshes) {
    const auto &submesh_aabb = submesh.bounding_box;
    const auto min = glm::vec3(submesh.transform * submesh_aabb.min_vector());
    const auto max = glm::vec3(submesh.transform * submesh_aabb.max_vector());

    bounding_box.update(min, max);
  }

  // Materials
  const auto &white_texture = SceneRenderer::get_white_texture();
  const auto num_materials = importer->scene->mNumMaterials;
  const std::span materials_span{importer->scene->mMaterials, num_materials};

  if (materials_span.empty()) {
    auto submesh_material =
        Material::construct_reference(*device, *default_shader);

    static constexpr auto default_roughness_and_albedo = 0.8F;
    auto vec_default_roughness_and_albedo = glm::vec3(0.8F);
    submesh_material->set("pc.albedo_colour", vec_default_roughness_and_albedo);
    submesh_material->set("pc.emission", 0.1F);
    submesh_material->set("pc.metalness", 0.1F);
    submesh_material->set("pc.roughness", default_roughness_and_albedo);
    submesh_material->set("pc.use_normal_map", 0.0F);

    submesh_material->set("albedo_map", white_texture);
    submesh_material->set("diffuse_map", white_texture);
    submesh_material->set("normal_map", white_texture);
    submesh_material->set("metallic_map", white_texture);
    submesh_material->set("roughness_map", white_texture);
    submesh_material->set("ao_map", white_texture);
    submesh_material->set("specular_map", white_texture);
    materials.push_back(submesh_material);

    return;
  }

  materials.resize(num_materials);

  std::size_t i = 0;
  for (const auto *ai_material : materials_span) {
    auto ai_material_name = ai_material->GetName();
    // convert to std::string
    const std::string material_name = ai_material_name.C_Str();

    auto submesh_material =
        Material::construct_reference(*device, *default_shader);
    materials[i++] = submesh_material;
    submesh_material->set("albedo_map", white_texture);
    submesh_material->set("diffuse_map", white_texture);
    submesh_material->set("normal_map", white_texture);
    submesh_material->set("metallic_map", white_texture);
    submesh_material->set("roughness_map", white_texture);
    submesh_material->set("ao_map", white_texture);
    submesh_material->set("specular_map", white_texture);

    aiString ai_tex_path;
    glm::vec4 albedo_colour{0.8F, 0.8F, 0.8F, 1.0F};
    float emission = 0.2F;
    if (aiColor3D ai_colour;
        ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, ai_colour) == AI_SUCCESS) {
      albedo_colour = {ai_colour.r, ai_colour.g, ai_colour.b, 1.0F};
    }

    if (aiColor3D ai_emission;
        ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, ai_emission) == AI_SUCCESS) {
      emission = ai_emission.r;
    }

    submesh_material->set("pc.albedo_colour", albedo_colour);
    submesh_material->set("pc.emission", emission);

    float shininess{};
    float metalness{0.0F};
    if (ai_material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS) {
      shininess = 80.0F; // Default value
    }

    if (ai_material->Get(AI_MATKEY_REFLECTIVITY, metalness) !=
        aiReturn_SUCCESS) {
      metalness = 0.0F;
    }

    handle_albedo_map(white_texture, ai_material, *submesh_material,
                      ai_tex_path);
    handle_normal_map(white_texture, ai_material, *submesh_material,
                      ai_tex_path);

    auto roughness = 1.0F - glm::sqrt(shininess / 100.0f);
    handle_roughness_map(white_texture, ai_material, *submesh_material,
                         ai_tex_path, roughness);
    handle_metalness_map(white_texture, ai_material, *submesh_material,
                         metalness);
  }
}

void Mesh::handle_albedo_map(const Texture &white_texture,
                             const aiMaterial *ai_material,
                             Material &submesh_material, aiString ai_tex_path) {
  const bool has_albedo_map =
      ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &ai_tex_path) ==
      AI_SUCCESS;
  auto fallback = !has_albedo_map;
  if (const std::string key = ai_tex_path.C_Str(); has_albedo_map) {
    bool texture = false;
    if (const auto *ai_texture_embedded =
            importer->scene->GetEmbeddedTexture(key.c_str())) {
      ensure(false, "No support for embedded textures");
    } else {
      mesh_owned_textures.try_emplace(key, read_texture_from_file_path(key));
      texture = true;
    }

    if (texture) {
      info("Has albedo map: {}", mesh_owned_textures.at(key)->get_file_path());
      const auto &text = mesh_owned_textures.at(key);
      submesh_material.set("albedo_map", *text);
      auto albedo_colour = glm::vec3(1.0F);
      submesh_material.set("pc.albedo_colour", albedo_colour);
    } else {
      fallback = true;
    }
  }

  if (fallback) {
    submesh_material.set("albedo_map", white_texture);
  }
}

void Mesh::handle_normal_map(const Texture &white_texture,
                             const aiMaterial *ai_material,
                             Material &submesh_material, aiString ai_tex_path) {
  auto has_normal_map = ai_material->GetTexture(aiTextureType_NORMALS, 0,
                                                &ai_tex_path) == AI_SUCCESS;
  auto fallback = !has_normal_map;
  if (const std::string key = ai_tex_path.C_Str(); has_normal_map) {
    bool texture = false;
    if (const auto *ai_texture_embedded =
            importer->scene->GetEmbeddedTexture(ai_tex_path.C_Str())) {
      ensure(false, "Embedded textures are not supported.");
    } else {
      mesh_owned_textures.try_emplace(key, read_texture_from_file_path(key));
      texture = true;
    }

    if (texture) {
      submesh_material.set("normal_map", *mesh_owned_textures.at(key));
      submesh_material.set("pc.use_normal_map", 1.0F);
      info("Has normal map: {}", mesh_owned_textures.at(key)->get_file_path());
    } else {
      fallback = true;
    }
  }

  if (fallback) {
    submesh_material.set("normal_map", white_texture);
    submesh_material.set("pc.use_normal_map", 0.0F);
  }
}

void Mesh::handle_roughness_map(const Texture &white_texture,
                                const aiMaterial *ai_material,
                                Material &submesh_material,
                                aiString ai_tex_path, float roughness) {
  aiString path; // Path to the texture
  bool has_roughness_texture = false;

  // Attempt to get the texture path for the metallic-roughness map
  // Assimp defines the key AI_MATKEY_TEXTURE_TYPE_METALNESS as the way to
  // access metalness maps, but for GLTF, we often use aiTextureType_UNKNOWN or
  // aiTextureType_METALNESS for metallic-roughness maps

  static auto load_texture = [=](auto type, auto &out_path) {
    if (type == aiTextureType_UNKNOWN)
      return ai_material->GetTexture(type, 0, &out_path, nullptr, nullptr,
                                     nullptr, nullptr, nullptr) == AI_SUCCESS;
    return ai_material->GetTexture(type, 0, &out_path) == AI_SUCCESS;
  };

  if (load_texture(aiTextureType_DIFFUSE_ROUGHNESS, path) ||
      load_texture(aiTextureType_METALNESS, path) ||
      load_texture(aiTextureType_UNKNOWN, path) ||
      load_texture(aiTextureType_SHININESS, path)) {
    std::string texture_path = path.C_Str();

    if (!texture_path.empty()) {
      // Load the texture from the path
      if (!mesh_owned_textures.contains(texture_path)) {
        mesh_owned_textures.try_emplace(
            texture_path, read_texture_from_file_path(texture_path));
      }
      has_roughness_texture = true;

      submesh_material.set("roughness_map",
                           *mesh_owned_textures.at(texture_path));
      submesh_material.set("pc.roughness", 1.0F);

      info("Has roughness texture: {}", texture_path);
    }
  }

  // Fallback to default white texture if no metallic-roughness map was found
  if (!has_roughness_texture) {
    submesh_material.set("roughness_map", white_texture);
    submesh_material.set("pc.roughness", roughness);
  }
}

void Mesh::handle_metalness_map(const Texture &white_texture,
                                const aiMaterial *ai_material,
                                Material &submesh_material, float metalness) {
  aiString path; // Path to the texture
  bool has_metalness_texture = false;

  // Attempt to get the texture path for the metallic-roughness map
  // Assimp defines the key AI_MATKEY_TEXTURE_TYPE_METALNESS as the way to
  // access metalness maps, but for GLTF, we often use aiTextureType_UNKNOWN or
  // aiTextureType_METALNESS for metallic-roughness maps
  if (ai_material->GetTexture(aiTextureType_METALNESS, 0, &path) ==
          AI_SUCCESS ||
      ai_material->GetTexture(aiTextureType_UNKNOWN, 0, &path, nullptr, nullptr,
                              nullptr, nullptr, nullptr) == AI_SUCCESS) {
    std::string texture_path = path.C_Str();

    if (!texture_path.empty()) {
      // Load the texture from the path
      if (!mesh_owned_textures.contains(texture_path)) {
        mesh_owned_textures.try_emplace(
            texture_path, read_texture_from_file_path(texture_path));
      }
      has_metalness_texture = true;

      submesh_material.set("metallic_map",
                           *mesh_owned_textures.at(texture_path));
      submesh_material.set("pc.metalness", 1.0F);

      info("Has metalness texture: {}", texture_path);
    }
  }

  // Fallback to default white texture if no metallic-roughness map was found
  if (!has_metalness_texture) {
    submesh_material.set("metallic_map", white_texture);
    submesh_material.set("pc.metalness", metalness);
  }
}

auto load_path_from_texture_path(const std::string_view texture_path) {
  static constexpr auto paths = std::array{"."};
  using recursive_iterator = std::filesystem::recursive_directory_iterator;
  static constexpr std::hash<std::string_view> hasher{};
  static std::unordered_map<std::size_t, std::filesystem::path> cache{};

  // Remove everything before and including App in texture_path
  const auto actual_path =
      FS::Path{texture_path.substr(texture_path.find("App") + 4)};

  const auto texture_path_hash = hasher(actual_path.filename().string());
  if (cache.contains(texture_path_hash)) {
    return cache.at(texture_path_hash);
  }

  for (const auto &path : paths) {
    for (const auto &entry :
         recursive_iterator(std::filesystem::absolute(path))) {
      if (entry.is_regular_file()) {
        const auto hash = hasher(entry.path().filename().string());
        cache.try_emplace(hash, entry.path());
      }
    }
  }

  if (cache.contains(texture_path_hash)) {
    return cache.at(texture_path_hash);
  }

  return FS::Path{};
}

auto Mesh::read_texture_from_file_path(const std::string &texture_path) const
    -> Scope<Texture> {
  using enum Core::ImageUsage;
  const auto path = load_path_from_texture_path(texture_path);
  return Texture::construct_shader(
      *device, {
                   .format = ImageFormat::UNORM_RGBA8,
                   .path = path,
                   .layout = ImageLayout::ShaderReadOnlyOptimal,
               });
}

} // namespace Core
