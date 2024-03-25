#include "pch/vkgpgpu_pch.hpp"

#include "Material.hpp"

#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "Config.hpp"
#include "Containers.hpp"
#include "Pipeline.hpp"
#include "SceneRenderer.hpp"

#include <string_view>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "reflection/ReflectionData.hpp"

namespace Core {

auto Material::construct(const Device &device, const Shader &shader)
    -> Scope<Material> {
  return Scope<Material>(new Material(device, shader));
}

auto Material::construct_reference(const Device &device, const Shader &shader)
    -> Ref<Material> {
  return Ref<Material>(new Material(device, shader));
}

auto Material::default_initialisation() -> void {
  static constexpr auto default_roughness_and_albedo = 0.8F;
  auto vec_default_roughness_and_albedo = glm::vec3(0.8F);
  set("pc.albedo_colour", vec_default_roughness_and_albedo);
  set("pc.emission", 0.1F);
  set("pc.metalness", 0.1F);
  set("pc.roughness", default_roughness_and_albedo);
  set("pc.use_normal_map", 0.0F);

  set("albedo_map", SceneRenderer::get_white_texture());
  set("diffuse_map", SceneRenderer::get_white_texture());
  set("normal_map", SceneRenderer::get_white_texture());
  set("metallic_map", SceneRenderer::get_white_texture());
  set("roughness_map", SceneRenderer::get_white_texture());
  set("ao_map", SceneRenderer::get_white_texture());
}

Material::Material(const Device &dev, const Shader &input_shader)
    : device(&dev), shader(&input_shader),
      write_descriptors(Config::frame_count),
      dirty_descriptor_sets(Config::frame_count, false) {
  initialise_constant_buffer();
}

auto Material::on_resize(const Extent<u32> &) -> void {
  initialise_constant_buffer();
  resident_descriptors.clear();
  resident_descriptor_arrays.clear();
  pending_descriptors.clear();
  texture_references.clear();
  image_references.clear();
  write_descriptors.resize(write_descriptors.size());
  dirty_descriptor_sets.resize(dirty_descriptor_sets.size());
  identifiers.clear();
}

auto Material::construct_buffers() -> void {}

auto Material::set(const std::string_view identifier, const void *data)
    -> bool {
  const auto found = find_uniform(identifier);
  if (!found)
    return false;

  const auto &value = *found;
  const auto &offset = value->get_offset();
  const auto &size = value->get_size();

  constant_buffer.write(data, size, offset);
  return true;
}

auto Material::find_resource(const std::string_view identifier)
    -> std::optional<const Reflection::ShaderResourceDeclaration *> {
  if (identifiers.contains(identifier)) {
    return &identifiers.at(identifier);
  }
  auto &&[shader_descriptor_sets, push_constant_ranges, constant_buffers,
          resources] = shader->get_reflection_data();
  if (resources.contains(identifier)) {
    identifiers.try_emplace(identifier, resources.at(identifier.data()));
    return &identifiers.at(identifier);
  }

  return std::nullopt;
}

auto Material::find_uniform(const std::string_view identifier) const
    -> std::optional<const Reflection::ShaderUniform *> {
  auto &shader_buffers = shader->get_reflection_data().constant_buffers;

  if (!shader_buffers.empty()) {
    const auto &buffer = (*shader_buffers.begin()).second;
    auto iterator = buffer.uniforms.find(identifier);
    if (iterator != buffer.uniforms.end()) {
      const auto &[_, value] = *iterator;
      return &value;
    }

    return std::nullopt;
  }
  return std::nullopt;
}

void Material::initialise_constant_buffer() {
  u64 size = 0;
  for (const auto &cb : shader->get_reflection_data().constant_buffers) {
    size += cb.second.size;
  }
  constant_buffer.set_size_and_reallocate(size);
  constant_buffer.fill_zero();
}

auto Material::invalidate_descriptor_sets() -> void {
  for (auto i = FrameIndex{0}; i < Config::frame_count; ++i) {
    dirty_descriptor_sets.at(i) = true;
  }
}

auto Material::invalidate() -> void {
  if (const auto &shader_descriptor_sets =
          shader->get_reflection_data().shader_descriptor_sets;
      !shader_descriptor_sets.empty()) {
    for (const auto &descriptor : resident_descriptors | std::views::values) {
      pending_descriptors.push_back(descriptor);
    }
  }
}

auto Material::bind_impl(const CommandBuffer &command_buffer,
                         const VkPipelineLayout &layout,
                         const VkPipelineBindPoint &bind_point, u32 frame,
                         VkDescriptorSet additional_set) const -> void {
  auto &[frame_sets] = descriptor_sets.at(frame);

  if (frame_sets.empty()) {
    return;
  }

  auto copy = frame_sets;
  if (additional_set != nullptr) {
    copy.push_back(additional_set);
  }

  vkCmdBindDescriptorSets(command_buffer.get_command_buffer(), bind_point,
                          layout, 0, static_cast<u32>(copy.size()), copy.data(),
                          0, nullptr);
}

void count_write_sets(const std::vector<VkWriteDescriptorSet> &writeSets,
                      u32 &image_count, u32 &buffer_count) {
  image_count = 0;  // Reset count for image/texture write sets
  buffer_count = 0; // Reset count for buffer write sets

  for (const auto &writeSet : writeSets) {
    switch (writeSet.descriptorType) {
    // Add cases for image/texture types
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      // Add more cases as needed for other image/texture types
      image_count++;
      break;

    // Add cases for buffer types
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      // Add more cases as needed for other buffer types
      buffer_count++;
      break;

      // Optionally handle other descriptor types here

    default:
      // Optionally handle unexpected descriptor types
      break;
    }
  }
}

auto Material::update_for_rendering(
    FrameIndex frame_index, const std::vector<std::vector<VkWriteDescriptorSet>>
                                &buffer_set_write_descriptors) -> void {
  auto &frame_write_descriptors = write_descriptors[frame_index];

  for (const auto &descriptor : resident_descriptors | std::views::values) {
    if (descriptor->type == PendingDescriptorType::Image2D) {
      auto &image = descriptor->image;
      if (descriptor->write_set.pImageInfo != nullptr &&
          image->get_descriptor_info().imageView !=
              descriptor->write_set.pImageInfo->imageView) {
        pending_descriptors.emplace_back(descriptor);
        invalidate_descriptor_sets();
      }
    }
  }

  std::vector<VkDescriptorImageInfo> array_image_infos;

  dirty_descriptor_sets[frame_index] = false;
  frame_write_descriptors.clear();

  u32 additional_buffer_writes = 0;
  if (!buffer_set_write_descriptors.empty()) {
    for (const auto &write : buffer_set_write_descriptors[frame_index]) {
      frame_write_descriptors.push_back(write);
    }

    for (const auto &pd : resident_descriptors | std::views::values) {
      if (pd->type == PendingDescriptorType::Buffer) {
        auto &buffer = pd->buffer;
        pd->buffer_info = buffer->get_descriptor_info();
        pd->write_set.pBufferInfo = &pd->buffer_info;
        frame_write_descriptors.push_back(pd->write_set);
        additional_buffer_writes++;
      }
    }
  }

  for (const auto &pd : resident_descriptors | std::views::values) {
    if (pd->type == PendingDescriptorType::Texture2D) {
      auto &texture = pd->texture;
      pd->image_info = texture->get_image().get_descriptor_info();
      pd->write_set.pImageInfo = &pd->image_info;
    } else if (pd->type == PendingDescriptorType::Image2D) {
      auto &image = pd->image;
      pd->image_info = image->get_descriptor_info();
      pd->write_set.pImageInfo = &pd->image_info;
    } else if (pd->type == PendingDescriptorType::TextureCube) {
      auto &image = pd->texture_cube;
      pd->image_info = image->get_descriptor_info();
      pd->write_set.pImageInfo = &pd->image_info;
    }
    frame_write_descriptors.push_back(pd->write_set);
  }

  for (const auto &pd : resident_descriptor_arrays | std::views::values) {
    if (pd->type == PendingDescriptorType::Texture2D) {
      for (const auto &texture : pd->textures) {
        array_image_infos.emplace_back(
            texture->get_image().get_descriptor_info());
      }
    }
    pd->write_set.pImageInfo = array_image_infos.data();
    pd->write_set.descriptorCount = static_cast<u32>(array_image_infos.size());
    frame_write_descriptors.push_back(pd->write_set);
  }

  static constexpr auto is_image = [](const auto &write_set) {
    return write_set.pImageInfo != nullptr;
  };

  std::unordered_map<u32, std::vector<VkWriteDescriptorSet>> split_by_type;
  u32 image_wds_count{};
  u32 buffer_wds_count{};
  count_write_sets(frame_write_descriptors, image_wds_count, buffer_wds_count);
  split_by_type.try_emplace(0, buffer_wds_count);
  split_by_type.try_emplace(1, image_wds_count);

  usize buffer_index = 0;
  usize image_index = 0;
  for (const auto &write_set : frame_write_descriptors) {
    if (is_image(write_set)) {
      split_by_type.at(1).at(image_index++) = write_set;
    } else {
      split_by_type.at(0).at(buffer_index++) = write_set;
    }
  }

  auto &current_sets = descriptor_sets[frame_index].descriptor_sets;
  current_sets = {};
  if (shader->has_descriptor_set(0)) {
    auto descriptor_set_0 = shader->allocate_descriptor_set(0);
    std::ranges::for_each(
        split_by_type.at(0),
        [&set = descriptor_set_0](VkWriteDescriptorSet &value) {
          value.dstSet = set.descriptor_sets.at(0);
        });
    vkUpdateDescriptorSets(device->get_device(),
                           static_cast<u32>(split_by_type.at(0).size()),
                           split_by_type.at(0).data(), 0, nullptr);

    current_sets.push_back(descriptor_set_0.descriptor_sets.at(0));
  }

  if (shader->has_descriptor_set(1)) {
    auto descriptor_set_1 = shader->allocate_descriptor_set(1);
    std::ranges::for_each(
        split_by_type.at(1),
        [&set = descriptor_set_1](VkWriteDescriptorSet &value) {
          value.dstSet = set.descriptor_sets.at(0);
        });
    vkUpdateDescriptorSets(device->get_device(),
                           static_cast<u32>(split_by_type.at(1).size()),
                           split_by_type.at(1).data(), 0, nullptr);

    current_sets.push_back(descriptor_set_1.descriptor_sets.at(0));
  }

  pending_descriptors.clear();
}

auto Material::set(std::string_view name, const Texture &texture) -> bool {
  const auto resource = find_resource(name);
  if (!resource)
    return false;

  const auto &found_resource = *resource;
  const u32 binding = found_resource->get_register();

  auto &textures = texture_references;

  if (binding < textures.size() && textures.at(binding) &&
      texture.hash() == textures.at(binding)->hash() &&
      resident_descriptors.contains(binding)) {
    return false;
  }

  if (binding >= textures.size()) {
    textures.resize(binding + 1);
  }
  textures.at(binding) = &texture;

  auto wds = shader->get_descriptor_set(name, 1);
  PendingDescriptor descriptor{
      .type = PendingDescriptorType::Texture2D,
      .write_set = *wds,
      .image_info = {},
      .buffer_info = {},
      .texture = textures.at(binding),
  };
  resident_descriptors[binding] =
      std::make_shared<PendingDescriptor>(descriptor);
  pending_descriptors.push_back(resident_descriptors.at(binding));

  invalidate_descriptor_sets();
  return true;
}

auto Material::set(const std::string_view name, const TextureCube &cube)
    -> bool {
  const auto resource = find_resource(name);
  if (!resource)
    return false;

  const auto &found_resource = *resource;

  const u32 binding = found_resource->get_register();
  auto &cubes = texture_cube_references;
  if (binding < cubes.size() && cubes.at(binding) &&
      resident_descriptors.contains(binding)) {
    return false;
  }

  const auto index = found_resource->get_register();
  if (index >= cubes.size()) {
    cubes.resize(static_cast<usize>(index) + 1);
  }
  cubes.at(index) = &cube;

  const auto *wds = shader->get_descriptor_set(name, 1);
  PendingDescriptor descriptor{
      .type = PendingDescriptorType::TextureCube,
      .write_set = *wds,
      .image_info = {},
      .buffer_info = {},
      .texture_cube = cubes.at(index),
  };
  resident_descriptors[binding] =
      std::make_shared<PendingDescriptor>(descriptor);
  pending_descriptors.push_back(resident_descriptors.at(binding));

  invalidate_descriptor_sets();
  return true;
}

auto Material::set(const std::string_view name, const Image &image) -> bool {
  const auto resource = find_resource(name);
  if (!resource)
    return false;

  const auto &found_resource = *resource;

  const u32 binding = found_resource->get_register();
  auto &images = image_references;
  if (binding < images.size() && images.at(binding) &&
      resident_descriptors.contains(binding)) {
    return false;
  }

  if (binding >= images.size()) {
    images.resize(static_cast<usize>(binding) + 1);
  }
  images.at(binding) = &image;

  const auto *wds = shader->get_descriptor_set(name, 1);
  PendingDescriptor descriptor{
      .type = PendingDescriptorType::Image2D,
      .write_set = *wds,
      .image_info = image.get_descriptor_info(),
      .buffer_info = {},
      .image = images.at(binding),
  };
  resident_descriptors[binding] =
      std::make_shared<PendingDescriptor>(descriptor);
  pending_descriptors.push_back(resident_descriptors.at(binding));

  invalidate_descriptor_sets();
  return true;
}

auto Material::set(std::string_view name, const Buffer &buffer) -> bool {
  const auto resource = find_resource(name);
  if (!resource)
    return false;

  const auto &found_resource = *resource;

  const u32 binding = found_resource->get_register();
  auto &buffers = buffer_references;
  if (binding < buffers.size() && buffers.at(binding) &&
      resident_descriptors.contains(binding)) {
    return false;
  }

  const auto index = found_resource->get_register();
  if (index >= buffers.size()) {
    buffers.resize(static_cast<usize>(index) + 1);
  }
  buffers.at(index) = &buffer;

  const auto *wds = shader->get_descriptor_set(name, 0);
  resident_descriptors[binding] =
      std::make_shared<PendingDescriptor>(PendingDescriptor{
          .type = PendingDescriptorType::Buffer,
          .write_set = *wds,
          .image_info = {},
          .texture = nullptr,
          .image = nullptr,
          .texture_cube = nullptr,
          .buffer = buffers[found_resource->get_register()],
      });
  pending_descriptors.push_back(resident_descriptors.at(binding));

  invalidate_descriptor_sets();
  return true;
}

} // namespace Core
