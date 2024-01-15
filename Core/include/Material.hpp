#pragma once

#include "Buffer.hpp"
#include "Config.hpp"
#include "Device.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

namespace Core {

class Material {
public:
  static auto construct(const Device &, const Shader &) -> Scope<Material>;

private:
  Material(const Device &, const Shader &);
  auto construct_buffers() -> void;
  auto construct_images() -> void;

  const Device *device{nullptr};
  const Shader *shader{nullptr};

  std::array<std::vector<Scope<Buffer>>, Config::frame_count>
      frame_indexed_buffers{};
  std::array<std::vector<Scope<Texture>>, Config::frame_count>
      frame_indexed_images{};
};

} // namespace Core
