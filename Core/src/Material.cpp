#include "pch/vkgpgpu_pch.hpp"

#include "Material.hpp"

#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "Config.hpp"
#include "Containers.hpp"
#include "Pipeline.hpp"

#include <string_view>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "reflection/ReflectionData.hpp"

namespace Core {

auto Material::construct(const Device &device, const Shader &shader)
    -> Scope<Material> {
  return Scope<Material>(new Material(device, shader));
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

  if (!buffer_set_write_descriptors.empty()) {
    for (const auto &write : buffer_set_write_descriptors[frame_index]) {
      frame_write_descriptors.push_back(write);
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
  split_by_type.try_emplace(0, 0);
  split_by_type.try_emplace(1, 0);

  for (const auto &write_set : frame_write_descriptors) {
    if (is_image(write_set)) {
      split_by_type.at(1).push_back(write_set);
    } else {
      split_by_type.at(0).push_back(write_set);
    }
  }

  auto descriptor_set_0 = shader->allocate_descriptor_set(0);
  auto descriptor_set_1 = shader->allocate_descriptor_set(1);
  std::ranges::for_each(split_by_type.at(0).begin(), split_by_type.at(0).end(),
                        [&set = descriptor_set_0](VkWriteDescriptorSet &value) {
                          value.dstSet = set.descriptor_sets.at(0);
                        });
  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<u32>(split_by_type.at(0).size()),
                         split_by_type.at(0).data(), 0, nullptr);

  std::ranges::for_each(split_by_type.at(1).begin(), split_by_type.at(1).end(),
                        [&set = descriptor_set_1](VkWriteDescriptorSet &value) {
                          value.dstSet = set.descriptor_sets.at(0);
                        });
  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<u32>(split_by_type.at(1).size()),
                         split_by_type.at(1).data(), 0, nullptr);

  descriptor_sets[frame_index].descriptor_sets = {
      descriptor_set_0.descriptor_sets.at(0),
      descriptor_set_1.descriptor_sets.at(0)};
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
  resident_descriptors[binding] = std::make_shared<PendingDescriptor>(
      PendingDescriptor{PendingDescriptorType::Texture2D,
                        *wds,
                        {},
                        textures.at(binding),
                        nullptr});
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

  const auto index = found_resource->get_register();
  if (index >= images.size()) {
    images.resize(static_cast<usize>(index) + 1);
  }
  images.at(index) = &image;

  const auto *wds = shader->get_descriptor_set(name, 1);
  resident_descriptors[binding] =
      std::make_shared<PendingDescriptor>(PendingDescriptor{
          PendingDescriptorType::Image2D,
          *wds,
          {},
          nullptr,
          images[found_resource->get_register()],
      });
  pending_descriptors.push_back(resident_descriptors.at(binding));

  invalidate_descriptor_sets();
  return true;
}

} // namespace Core
