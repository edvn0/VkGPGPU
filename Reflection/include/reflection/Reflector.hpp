#pragma once

#include "Types.hpp"
#include "core/Forward.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

namespace Reflection {

class Reflector {
  struct CompilerImpl;

public:
  explicit Reflector(Core::Shader &);
  ~Reflector();
  auto reflect(std::vector<VkDescriptorSetLayout> &) -> void;

private:
  Core::Shader &shader;
  Core::Scope<CompilerImpl> impl{nullptr};
};

} // namespace Reflection