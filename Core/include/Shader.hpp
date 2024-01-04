#pragma once

#include "Types.hpp"

#include <filesystem>
#include <vulkan/vulkan.h>

namespace Core {

class Shader {
public:
  enum class Type : u8 {
    Compute,
    Vertex,
    Fragment,
  };

  explicit Shader(const std::filesystem::path &path);
  ~Shader();

  [[nodiscard]] auto get_shader_module() const -> VkShaderModule {
    return shader_module;
  }

private:
  VkShaderModule shader_module{};
};

} // namespace Core