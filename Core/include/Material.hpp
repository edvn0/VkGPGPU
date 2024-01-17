#pragma once

#include "Buffer.hpp"
#include "Concepts.hpp"
#include "Config.hpp"
#include "Device.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

#include <BufferSet.hpp>

#include "reflection/ReflectionData.hpp"

namespace Core {

class Material {
public:
  static auto construct(const Device &, const Shader &) -> Scope<Material>;

  auto set(const std::string_view identifier, const IsBuiltin auto &value)
      -> bool {
    const auto &copy = value;
    return set(identifier, static_cast<const void *>(&copy));
  }

  [[nodiscard]] auto set(std::string_view, const Buffer &) -> bool;
  [[nodiscard]] auto set(std::string_view, const Image &) -> bool;
  [[nodiscard]] auto get_constant_buffer() const -> const auto & {
    return constant_buffer;
  }

private:
  Material(const Device &, const Shader &);
  auto construct_buffers() -> void;
  auto construct_images() -> void;

  auto set(std::string_view, const void *data) -> bool;
  [[nodiscard]] auto find_resource(std::string_view) const
      -> std::optional<const Reflection::ShaderResourceDeclaration *>;
  [[nodiscard]] auto find_uniform(std::string_view) const
      -> std::optional<const Reflection::ShaderUniform *>;

  const Device *device{nullptr};
  const Shader *shader{nullptr};

  BufferSet<Buffer::Type::Uniform> uniform_buffers;
  BufferSet<Buffer::Type::Storage> storage_buffers;
  std::vector<Scope<Texture>> textures;

  DataBuffer constant_buffer{};
  void initialise_constant_buffer();
};

} // namespace Core
