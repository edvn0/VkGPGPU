#include "pch/vkgpgpu_pch.hpp"

#include "Material.hpp"

#include "Containers.hpp"

#include <string_view>

#include "reflection/ReflectionData.hpp"

namespace Core {

auto Material::construct(const Device &device, const Shader &shader)
    -> Scope<Material> {

  // Since we have a private constructor, make_scope does not work
  return Scope<Material>(new Material(device, shader));
}

Material::Material(const Device &dev, const Shader &input_shader)
    : device(&dev), shader(&input_shader), uniform_buffers(dev),
      storage_buffers(dev) {
  initialise_constant_buffer();
}

auto Material::construct_buffers() -> void {

  auto reflection_data = shader->get_reflection_data();
}

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

auto Material::find_resource(const std::string_view identifier) const
    -> std::optional<const Reflection::ShaderResourceDeclaration *> {
  static std::unordered_map<std::string_view,
                            Reflection::ShaderResourceDeclaration>
      identifiers{};

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

} // namespace Core
