#pragma once

#include "Device.hpp"
#include "Types.hpp"

#include <filesystem>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "reflection/ReflectionData.hpp"

namespace Core {

class Shader {
public:
  enum class Type : u8 {
    Compute,
    Vertex,
    Fragment,
  };

  ~Shader();

  [[nodiscard]] auto get_shader_module() const -> VkShaderModule {
    return shader_module;
  }

  [[nodiscard]] auto get_code(Type t = Type::Compute) const -> const auto & {
    return parsed_spirv_per_stage.at(t);
  }

  [[nodiscard]] auto get_descriptor_set_layouts() const -> const auto & {
    return descriptor_set_layouts;
  }
  [[nodiscard]] auto get_push_constant_ranges() const -> const auto & {
    return push_constant_ranges;
  }

  [[nodiscard]] auto get_device() const -> const Device & { return device; }
  [[nodiscard]] auto get_name() const -> const std::string & { return name; }
  [[nodiscard]] auto get_reflection_data() const -> const auto & {
    return reflection_data;
  }

  [[nodiscard]] auto allocate_descriptor_set(u32 set) const
      -> Reflection::MaterialDescriptorSet;
  [[nodiscard]] auto get_descriptor_set(std::string_view descriptor_name,
                                        std::uint32_t set) const
      -> const VkWriteDescriptorSet *;

  [[nodiscard]] auto hash() const -> usize;
  [[nodiscard]] auto has_descriptor_set(u32 set) const -> bool;

  static auto construct(const Device &device, const std::filesystem::path &path)
      -> Scope<Shader>;

private:
  explicit Shader(const Device &device, const std::filesystem::path &path,
                  Type);

  const Device &device;
  std::string name{};
  Type type;
  VkShaderModule shader_module{};
  Reflection::ReflectionData reflection_data{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
  std::vector<VkPushConstantRange> push_constant_ranges{};
  std::unordered_map<Type, std::string> parsed_spirv_per_stage{};

  void create_descriptor_set_layouts();
  void create_push_constant_ranges();
};

} // namespace Core
