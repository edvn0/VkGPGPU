#pragma once

#include "ReflectionData.hpp"
#include "Types.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "core/Forward.hpp"

namespace Reflection {

class Reflector {
  struct CompilerImpl;

public:
  explicit Reflector(Core::Shader &);
  ~Reflector();
  auto reflect(std::vector<VkDescriptorSetLayout> &,
               ReflectionData &output) const -> void;

private:
  Core::Shader &shader;
  Core::Scope<CompilerImpl> impl{nullptr};
};

} // namespace Reflection
