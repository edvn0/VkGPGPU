#pragma once

#include "Device.hpp"
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

  explicit Shader(const Device &device, const std::filesystem::path &path);
  ~Shader();

  [[nodiscard]] auto get_shader_module() const -> VkShaderModule {
    return shader_module;
  }

  [[nodiscard]] auto get_code(Type type = Type::Compute) const -> const auto & {
    return parsed_spirv_per_stage.at(type);
  }

  [[nodiscard]] auto get_descriptor_set_layouts() const -> const auto & {
    return descriptor_set_layouts;
  }

  [[nodiscard]] auto get_device() const -> const Device & { return device; }
  [[nodiscard]] auto get_name() const -> const std::string & { return name; }

private:
  const Device &device;
  std::string name {};
  VkShaderModule shader_module{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
  std::unordered_map<Type, std::string> parsed_spirv_per_stage{};
};

} // namespace Core