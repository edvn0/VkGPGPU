#pragma once

#include "TextureCube.hpp"
#include "Types.hpp"

namespace Core {

struct SceneEnvironment {
  Ref<Core::TextureCube> radiance_texture{nullptr};
  Ref<Core::TextureCube> irradiance_texture{nullptr};
};

} // namespace Core
