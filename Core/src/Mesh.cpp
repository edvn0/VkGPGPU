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

auto create_texture_from_raw_data(const Device *device, const byte *data,
                                  i32 width, i32 height) -> Scope<Texture> {
  DataBuffer buffer{width * height * 4 * sizeof(byte)};
  buffer.write(data, width * height * 4 * sizeof(byte));

  return Texture::construct_from_buffer(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .extent = Extent<i32>(width, height).as<u32>(),
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .address_mode = SamplerAddressMode::ClampToEdge,
          .border_color = SamplerBorderColor::FloatOpaqueWhite,
      },
      std::move(buffer));
}

auto create_texture_from_memory(const Device *device, const aiTexel *data,
                                size_t size) -> Scope<Texture> {
  int width{};
  int height{};
  int channels{};
  auto *raw_pixels =
      stbi_load_from_memory(std::bit_cast<u8 *>(data),
                            static_cast<i32>(size * 4 * sizeof(unsigned char)),
                            &width, &height, &channels, STBI_rgb_alpha);

  if (!raw_pixels) {
    return nullptr;
  }

  auto texture =
      create_texture_from_raw_data(device, raw_pixels, width, height);

  stbi_image_free(raw_pixels);

  return texture;
}

auto create_texture_from_raw_data(const Device *device, const aiTexel *data,
                                  i32 width, i32 height) -> Scope<Texture> {
  DataBuffer buffer{width * height * 4 * 4 * sizeof(byte)};
  buffer.write(std::bit_cast<u8 *>(data),
               width * height * 4 * 4 * sizeof(byte));

  return Texture::construct_from_buffer(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .extent = Extent<i32>(width, height).as<u32>(),
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .address_mode = SamplerAddressMode::ClampToEdge,
          .border_color = SamplerBorderColor::FloatOpaqueWhite,
      },
      std::move(buffer));
}

// Select the kinds of messages you want to receive on this log stream
static constexpr u32 severity = Assimp::Logger::Debugging |
                                Assimp::Logger::Info | Assimp::Logger::Err |
                                Assimp::Logger::Warn;

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

struct ImporterImpl {
  Scope<Assimp::Importer> importer{nullptr};
  std::unordered_map<aiNode *, std::vector<u32>> node_map;
  const aiScene *scene;
};

auto Mesh::Deleter::operator()(ImporterImpl *impl) const -> void {
  delete impl;
}

namespace {
constexpr auto to_mat4_from_assimp(const aiMatrix4x4 &matrix) -> glm::mat4 {
  glm::mat4 result{};
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

  const auto local_transform = to_mat4_from_assimp(node->mTransformation);
  const auto transform = parent_transform * local_transform;
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
    aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals |
    aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes |
    aiProcess_GenBoundingBoxes | aiProcess_GenUVCoords |
    aiProcess_OptimizeGraph | aiProcess_JoinIdenticalVertices |
    aiProcess_ValidateDataStructure | aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials | aiProcess_FindInvalidData |
    aiProcess_FindDegenerates | aiProcess_FindInstances | aiProcess_SortByPType;

auto Mesh::import_from(const Device &device, const FS::Path &file_path)
    -> Scope<Mesh> {
  return Scope<Mesh>(new Mesh{device, file_path});
}

auto Mesh::reference_import_from(const Device &device,
                                 const FS::Path &file_path)
    -> const Ref<Mesh> & {
  const std::string key = file_path.string();
  if (mesh_cache.contains(key)) {
    return mesh_cache.at(key);
  }

  auto &&[value, could] =
      mesh_cache.try_emplace(key, Ref<Mesh>(new Mesh{device, key}));

  if (!could) {
    throw NotFoundException{fmt::format("Could not insert mesh at {}", key)};
  }

  return mesh_cache.at(key);
}

// Quite hacky. Works for now :)
auto Mesh::get_cube() -> const Ref<Mesh> & {
  static const auto &mesh = mesh_cache.at(FS::model("cube.gltf").string());
  return mesh;
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
    auto error_string = importer->importer->GetErrorString();
    error("Mesh: Failed to load mesh file: {0}. Reason: {1}",
          file_path.string(), error_string);
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

    auto rotation =
        glm::rotate(glm::mat4{1.0F}, glm::radians(90.0F), glm::vec3{1, 0, 0});

    // Vertices
    auto &submesh_aabb = submesh.bounding_box;
    for (std::size_t i = 0; i < mesh->mNumVertices; i++) {
      Vertex vertex{};
      vertex.pos = {
          mesh->mVertices[i].x,
          mesh->mVertices[i].y,
          mesh->mVertices[i].z,
      };
      vertex.pos = rotation * glm::vec4{vertex.pos, 1.0F};
      submesh_aabb.update(vertex.pos);
      vertex.normals = {
          mesh->mNormals[i].x,
          mesh->mNormals[i].y,
          mesh->mNormals[i].z,
      };
      vertex.normals = rotation * glm::vec4{vertex.normals, 1.0F};

      if (mesh->HasTangentsAndBitangents()) {
        vertex.tangents = {
            mesh->mTangents[i].x,
            mesh->mTangents[i].y,
            mesh->mTangents[i].z,
        };
        vertex.bitangents = {
            mesh->mBitangents[i].x,
            mesh->mBitangents[i].y,
            mesh->mBitangents[i].z,
        };
      }

      if (mesh->HasTextureCoords(0)) {
        vertex.uvs = {
            mesh->mTextureCoords[0][i].x,
            mesh->mTextureCoords[0][i].y,
        };
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

  traverse_nodes(submeshes, importer, importer->scene->mRootNode);

  vertex_buffer = Buffer::construct(*device, vertices.size() * sizeof(Vertex),
                                    Buffer::Type::Vertex, 0);
  vertex_buffer->write(std::span{vertices});
  index_buffer = Buffer::construct(*device, indices.size() * sizeof(Index),
                                   Buffer::Type::Index, 0);
  index_buffer->write(std::span{indices});

  static constexpr auto transform_aabb = [](const auto &aabb,
                                            const auto &transform) {
    std::vector<glm::vec3> corners;

    // Define all eight corners of the AABB
    const glm::vec3 &min = aabb.min();
    const glm::vec3 &max = aabb.max();
    corners.push_back(transform * glm::vec4(min.x, min.y, min.z, 1.0));
    corners.push_back(transform * glm::vec4(max.x, min.y, min.z, 1.0));
    corners.push_back(transform * glm::vec4(min.x, max.y, min.z, 1.0));
    corners.push_back(transform * glm::vec4(max.x, max.y, min.z, 1.0));
    corners.push_back(transform * glm::vec4(min.x, min.y, max.z, 1.0));
    corners.push_back(transform * glm::vec4(max.x, min.y, max.z, 1.0));
    corners.push_back(transform * glm::vec4(min.x, max.y, max.z, 1.0));
    corners.push_back(transform * glm::vec4(max.x, max.y, max.z, 1.0));

    // Calculate new AABB in world space
    auto newMin = glm::vec3(std::numeric_limits<float>::max());
    auto newMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto &corner : corners) {
      newMin = glm::min(newMin, glm::vec3(corner));
      newMax = glm::max(newMax, glm::vec3(corner));
    }

    return AABB{newMin, newMax};
  };
  glm::vec3 globalMin(std::numeric_limits<float>::max());
  glm::vec3 globalMax(std::numeric_limits<float>::lowest());
  for (const auto &submesh : submeshes) {

    const auto &submesh_aabb = submesh.bounding_box;
    AABB transformedAABB = transform_aabb(submesh_aabb, submesh.transform);
    globalMin = glm::min(globalMin, transformedAABB.min());
    globalMax = glm::max(globalMax, transformedAABB.max());
  }

  bounding_box = AABB(globalMin, globalMax);

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
    materials.push_back(submesh_material);

    Assimp::DefaultLogger::kill();
    return;
  }

  materials = {};

  std::size_t i = 0;
  for (const auto *ai_material : materials_span) {
    auto ai_material_name = ai_material->GetName();
    // convert to std::string
    const std::string material_name = ai_material_name.C_Str();

    // Gltf erroneously adds a material in some cases. Don't know why.
    if (material_name.empty())
      continue;

    auto submesh_material =
        Material::construct_reference(*device, *default_shader);
    submesh_material->set("albedo_map", white_texture);
    submesh_material->set("diffuse_map", white_texture);
    submesh_material->set("normal_map", white_texture);
    submesh_material->set("metallic_map", white_texture);
    submesh_material->set("roughness_map", white_texture);
    submesh_material->set("ao_map", white_texture);

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

    float shininess{0.0F};
    float metalness{0.0F};
    if (ai_material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS) {
      shininess = 80.0F; // Default value
    }

    if (ai_material->Get(AI_MATKEY_REFLECTIVITY, metalness) !=
        aiReturn_SUCCESS) {
      metalness = 0.0F;
    }

    handle_albedo_map(ai_material, *submesh_material, ai_tex_path);
    handle_normal_map(ai_material, *submesh_material, ai_tex_path);

    auto roughness = 1.0F - glm::sqrt(shininess / 250.0f);
    handle_roughness_map(ai_material, *submesh_material, ai_tex_path,
                         roughness);
    handle_metalness_map(ai_material, *submesh_material, metalness);

    materials.push_back(std::move(submesh_material));
  }

  if (materials.empty()) {
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
    materials.push_back(submesh_material);

    Assimp::DefaultLogger::kill();
    return;
  }

  Assimp::DefaultLogger::kill();
}

void Mesh::handle_albedo_map(const aiMaterial *ai_material,
                             Material &submesh_material, aiString ai_tex_path) {
  const bool has_albedo_map =
      ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &ai_tex_path) ==
      AI_SUCCESS;
  auto fallback = !has_albedo_map;
  if (const std::string key = ai_tex_path.C_Str(); has_albedo_map) {
    bool texture_loaded = false;
    if (const aiTexture *ai_texture_embedded =
            importer->scene->GetEmbeddedTexture(key.c_str())) {
      if (ai_texture_embedded->mHeight == 0) {
        auto texture = create_texture_from_memory(
            device, ai_texture_embedded->pcData, ai_texture_embedded->mWidth);
        mesh_owned_textures.try_emplace(key, std::move(texture));
        texture_loaded = true;
      } else {
        auto texture = create_texture_from_raw_data(
            device, ai_texture_embedded->pcData, ai_texture_embedded->mWidth,
            ai_texture_embedded->mHeight);
        mesh_owned_textures.try_emplace(key, std::move(texture));
        texture_loaded = true;
      }
    } else {
      mesh_owned_textures.try_emplace(key, read_texture_from_file_path(key));
      texture_loaded = true;
    }

    if (texture_loaded) {
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
    submesh_material.set("albedo_map", SceneRenderer::get_white_texture());
  }
}

void Mesh::handle_normal_map(const aiMaterial *ai_material,
                             Material &submesh_material, aiString ai_tex_path) {
  auto has_normal_map = ai_material->GetTexture(aiTextureType_NORMALS, 0,
                                                &ai_tex_path) == AI_SUCCESS;
  auto fallback = !has_normal_map;
  if (const std::string key = ai_tex_path.C_Str(); has_normal_map) {
    bool texture_loaded = false;
    if (const aiTexture *ai_texture_embedded =
            importer->scene->GetEmbeddedTexture(key.c_str())) {
      // Handle embedded texture.
      // Check if the embedded texture is uncompressed (RAW) or compressed.
      if (ai_texture_embedded->mHeight == 0) {
        // The embedded texture is compressed (e.g., PNG, JPG).
        // Use your method to create a texture from the compressed data in
        // memory.
        auto texture = create_texture_from_memory(
            device, ai_texture_embedded->pcData, ai_texture_embedded->mWidth);
        mesh_owned_textures.try_emplace(key, std::move(texture));
        texture_loaded = true;
      } else {
        // The embedded texture is uncompressed (RAW).
        // mWidth and mHeight specify the dimensions, and pcData contains the
        // pixel data.
        auto texture = create_texture_from_raw_data(
            device, ai_texture_embedded->pcData, ai_texture_embedded->mWidth,
            ai_texture_embedded->mHeight);
        mesh_owned_textures.try_emplace(key, std::move(texture));
        texture_loaded = true;
      }
    } else {
      // Texture is not embedded, read from file path.
      mesh_owned_textures.try_emplace(key, read_texture_from_file_path(key));
      texture_loaded = true;
    }

    if (texture_loaded) {
      submesh_material.set("normal_map", *mesh_owned_textures.at(key));
      submesh_material.set("pc.use_normal_map", 1.0F);
      info("Has normal map: {}", mesh_owned_textures.at(key)->get_file_path());
    } else {
      fallback = true;
    }
  }

  if (fallback) {
    submesh_material.set("normal_map", SceneRenderer::get_white_texture());
    submesh_material.set("pc.use_normal_map", 0.0F);
  }
}

static constexpr auto load_texture = [](const auto *ai_material, auto type,
                                        auto &out_path) {
  if (type == aiTextureType_UNKNOWN)
    return ai_material->GetTexture(type, 0, &out_path, nullptr, nullptr,
                                   nullptr, nullptr, nullptr) == AI_SUCCESS;
  return ai_material->GetTexture(type, 0, &out_path) == AI_SUCCESS;
};

void Mesh::handle_roughness_map(const aiMaterial *ai_material,
                                Material &submesh_material,
                                aiString ai_tex_path, float roughness) {
  aiString path; // Path to the texture
  bool has_roughness_texture = false;

  // Attempt to get the texture path for the metallic-roughness map
  // Assimp defines the key AI_MATKEY_TEXTURE_TYPE_METALNESS as the way to
  // access metalness maps, but for GLTF, we often use aiTextureType_UNKNOWN or
  // aiTextureType_METALNESS for metallic-roughness maps

  if (load_texture(ai_material, aiTextureType_DIFFUSE_ROUGHNESS, path) ||
      load_texture(ai_material, aiTextureType_METALNESS, path) ||
      load_texture(ai_material, aiTextureType_UNKNOWN, path) ||
      load_texture(ai_material, aiTextureType_SHININESS, path)) {
    std::string texture_path = path.C_Str();

    if (!texture_path.empty()) {
      try {
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
      } catch (const std::exception &exc) {
        error("Exception: {}", exc);
        has_roughness_texture = false;
      }
    }
  }

  // Fallback to default white texture if no metallic-roughness map was found
  if (!has_roughness_texture) {
    submesh_material.set("roughness_map", SceneRenderer::get_white_texture());
    submesh_material.set("pc.roughness", roughness);
  }
}

void Mesh::handle_metalness_map(const aiMaterial *ai_material,
                                Material &submesh_material, float metalness) {
  aiString path; // Path to the texture
  bool has_metalness_texture = false;

  // Attempt to get the texture path for the metallic-roughness map
  // Assimp defines the key AI_MATKEY_TEXTURE_TYPE_METALNESS as the way to
  // access metalness maps, but for GLTF, we often use aiTextureType_UNKNOWN or
  // aiTextureType_METALNESS for metallic-roughness maps
  if (load_texture(ai_material, aiTextureType_METALNESS, path) ||
      load_texture(ai_material, aiTextureType_UNKNOWN, path)) {
    std::string texture_path = path.C_Str();

    if (!texture_path.empty()) {
      try {

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
      } catch (const std::exception &exc) {
        error("Exception: {}", exc);
        has_metalness_texture = false;
      }
    }
  }

  // Fallback to default white texture if no metallic-roughness map was found
  if (!has_metalness_texture) {
    submesh_material.set("metallic_map", SceneRenderer::get_white_texture());
    submesh_material.set("pc.metalness", metalness);
  }
}

auto load_path_from_texture_path(const std::string_view texture_path) {
  static constexpr auto paths = std::array{"."};
  using recursive_iterator = std::filesystem::recursive_directory_iterator;
  static constexpr std::hash<std::string_view> hasher{};
  static std::unordered_map<std::size_t, std::filesystem::path> cache{};

  // Remove everything before and including App in texture_path
  FS::Path actual_path;
  if (texture_path.find("App") != std::string::npos) {
    actual_path = FS::Path{texture_path.substr(texture_path.find("App") + 4)};
  } else {
    actual_path = FS::Path{texture_path};
  }

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
  if (path.empty())
    return nullptr;

  static std::mutex mutex;
  std::unique_lock lock{mutex};
  return Texture::construct_shader(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .path = path,
          .usage = ImageUsage::ColourAttachment | ImageUsage::Sampled |
                   ImageUsage::TransferSrc | ImageUsage::TransferDst,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .mip_generation = MipGeneration(MipGenerationStrategy::FromSize),
      });
}

} // namespace Core
