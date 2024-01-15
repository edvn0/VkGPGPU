#include "Material.hpp"

#include "pch/vkgpgpu_pch.hpp"

namespace Core {

auto Material::construct(const Device &device, const Shader &shader)
    -> Scope<Material> {

  // Since we have a private constructor, make_scope does not work
  return Scope<Material>(new Material(device, shader));
}

Material::Material(const Device &dev, const Shader &input_shader)
    : device(&dev), shader(&input_shader) {}

auto Material::construct_buffers() -> void {

  auto reflection_data = shader->get_reflection_data();
}

} // namespace Core
