#pragma once

#include "Device.hpp"
#include "Types.hpp"

#include <filesystem>
#include <unordered_set>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "compilation/ShaderCompiler.hpp"
#include "reflection/ReflectionData.hpp"

namespace Core {

class Shader {
public:
  enum class Type : u8 {
    Compute,
    Vertex,
    Fragment,
  };
  struct PathShaderType {
    const std::filesystem::path path;
    const Type type;

    auto operator<=>(const PathShaderType &other) const {
      return type <=> other.type;
    }
    auto operator==(const PathShaderType &other) const {
      return type == other.type;
    }
  };

  struct Hasher {
    using is_transparent = void;
    auto operator()(const PathShaderType &type) const noexcept -> usize {
      static std::hash<u32> hasher;
      return hasher(static_cast<u32>(type.type));
    }
  };

  explicit Shader(
      const Device &device,
      const std::unordered_set<PathShaderType, Hasher, std::equal_to<>> &);
  explicit Shader(const Device &, std::unordered_map<Type, std::vector<u32>>,
                  std::string_view name = "");

  ~Shader();
  auto on_resize(const Extent<u32> &) -> void {}

  [[nodiscard]] auto get_shader_module(Type t = Type::Compute) const
      -> std::optional<VkShaderModule> {
    return shader_modules.contains(t) ? shader_modules.at(t)
                                      : std::optional<VkShaderModule>{};
  }

  [[nodiscard]] auto get_code(Type t = Type::Compute) const
      -> std::optional<std::string> {
    // First check parsed_spirv_per_stage_u32
    if (parsed_spirv_per_stage_u32.contains(t)) {
      std::vector<u32> spirv = parsed_spirv_per_stage_u32.at(t);
      return std::string(std::bit_cast<const char *>(spirv.data()),
                         spirv.size() * sizeof(u32));
    }

    return parsed_spirv_per_stage.contains(t) ? parsed_spirv_per_stage.at(t)
                                              : std::optional<std::string>{};
  }

  [[nodiscard]] auto get_descriptor_set_layouts() const -> const auto & {
    return descriptor_set_layouts;
  }
  [[nodiscard]] auto get_push_constant_ranges() const -> const auto & {
    return reflection_data.push_constant_ranges;
  }

  [[nodiscard]] auto get_device() const -> const Device & { return device; }
  [[nodiscard]] auto get_name() const -> const std::string & { return name; }
  [[nodiscard]] auto get_reflection_data() const -> const auto & {
    return reflection_data;
  }

  [[nodiscard]] auto allocate_descriptor_set(u32 set) const
      -> Reflection::MaterialDescriptorSet;
  [[nodiscard]] auto get_descriptor_set(std::string_view, u32) const
      -> const VkWriteDescriptorSet *;

  [[nodiscard]] auto hash() const -> usize;
  [[nodiscard]] auto has_descriptor_set(u32 set) const -> bool;

  static auto compile_graphics(const Device &, const std::filesystem::path &,
                               const std::filesystem::path &) -> Ref<Shader>;
  static auto compile_compute(const Device &, const std::filesystem::path &)
      -> Ref<Shader>;
  static auto compile_graphics_scoped(const Device &,
                                      const std::filesystem::path &,
                                      const std::filesystem::path &)
      -> Scope<Shader>;
  static auto compile_compute_scoped(const Device &,
                                     const std::filesystem::path &)
      -> Scope<Shader>;

  static auto
  initialise_compiler(const Device &device,
                      const Compilation::ShaderCompilerConfiguration &) -> void;

private:
  const Device &device;
  std::string name{};
  usize hash_value{0};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
  Reflection::ReflectionData reflection_data{};
  std::unordered_map<Type, VkShaderModule> shader_modules{};
  std::unordered_map<Type, std::string> parsed_spirv_per_stage{};
  std::unordered_map<Type, std::vector<u32>> parsed_spirv_per_stage_u32{};

  void create_descriptor_set_layouts();

  // Using forward declaration to avoid circular dependency of ShaderCompiler
  static inline Scope<Compilation::ShaderCompiler> compiler{nullptr};
};

} // namespace Core
