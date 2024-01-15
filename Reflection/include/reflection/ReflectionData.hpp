#pragma once

#include "Filesystem.hpp"
#include "Types.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Reflection {

struct StringLikeHasher {
  using is_transparent = void;
  [[nodiscard]] size_t operator()(const char *txt) const {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] size_t operator()(std::string_view txt) const {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] size_t operator()(const std::string &txt) const {
    return std::hash<std::string>{}(txt);
  }
};

struct Uniform {
  Core::u32 binding{0};
  Core::u32 set{0};
};

enum class ShaderUniformType : std::uint8_t {
  None = 0,
  Bool,
  Int,
  UInt,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Mat3,
  Mat4,
  IVec2,
  IVec3,
  IVec4,
};

class ShaderUniform {
public:
  ShaderUniform() = default;
  ShaderUniform(std::string_view name_view, ShaderUniformType input_type,
                Core::u32 input_size, Core::u32 input_offset)
      : name(name_view), type(input_type), size(input_size),
        offset(input_offset){};

  [[nodiscard]] auto get_name() const -> const std::string & { return name; }
  [[nodiscard]] auto get_type() const -> ShaderUniformType { return type; }
  [[nodiscard]] auto get_size() const -> Core::u32 { return size; }
  [[nodiscard]] auto get_offset() const -> Core::u32 { return offset; }

private:
  std::string name;
  ShaderUniformType type{ShaderUniformType::None};
  Core::u32 size{0};
  Core::u32 offset{0};
};

struct ShaderUniformBuffer {
  std::string name;
  Core::u32 index{0};
  Core::u32 binding_point{0};
  Core::u32 size{0};
  std::vector<ShaderUniform> uniforms{};
};

struct ShaderStorageBuffer {
  std::string name;
  Core::u32 index{0};
  Core::u32 binding_point{0};
  Core::u32 size{0};
};

struct ShaderBuffer {
  std::string name;
  Core::u32 size{0};
  std::unordered_map<std::string, ShaderUniform> uniforms;
};

struct UniformBuffer {
  VkDescriptorBufferInfo descriptor{};
  Core::u32 size{0};
  Core::u32 binding_point{0};
  std::string name;
  VkShaderStageFlagBits shader_stage{VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
};

struct StorageBuffer {
  VkDescriptorBufferInfo descriptor{};
  Core::u32 size{0};
  Core::u32 binding_point{0};
  std::string name;
  VkShaderStageFlagBits shader_stage{VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
};

struct ImageSampler {
  Core::u32 binding_point{0};
  Core::u32 descriptor_set{0};
  Core::u32 array_size{0};
  std::string name;
  VkShaderStageFlagBits shader_stage{VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
};

struct PushConstantRange {
  Core::u32 offset{0};
  Core::u32 size{0};
  VkShaderStageFlags shader_stage{};
};

struct ShaderDescriptorSet {
  std::unordered_map<Core::u32, UniformBuffer> uniform_buffers{};
  std::unordered_map<Core::u32, StorageBuffer> storage_buffers{};
  std::unordered_map<Core::u32, ImageSampler> sampled_images{};
  std::unordered_map<Core::u32, ImageSampler> storage_images{};
  std::unordered_map<Core::u32, ImageSampler> separate_textures{};
  std::unordered_map<Core::u32, ImageSampler> separate_samplers{};

  std::unordered_map<std::string, VkWriteDescriptorSet> write_descriptor_sets{};
};

class ShaderResourceDeclaration final {
public:
  ~ShaderResourceDeclaration() = default;
  ShaderResourceDeclaration() = default;
  ShaderResourceDeclaration(std::string input_name, Core::u32 reg,
                            Core::u32 input_count)
      : name(std::move(input_name)), resource_register(reg),
        count(input_count) {}

  [[nodiscard]] auto get_name() const -> const std::string & { return name; }
  [[nodiscard]] auto get_register() const -> Core::u32 {
    return resource_register;
  }
  [[nodiscard]] auto get_count() const -> Core::u32 { return count; }

private:
  std::string name;
  Core::u32 resource_register{0};
  Core::u32 count{0};
};

enum class ShaderInputOrOutput : std::uint8_t {
  Input,
  Output,
};

struct ShaderInOut {
  Core::u32 location{0};
  std::string name;
  ShaderUniformType type;
};

struct ReflectionData {
  std::vector<ShaderDescriptorSet> shader_descriptor_sets{};
  std::vector<PushConstantRange> push_constant_ranges{};
  std::unordered_map<std::string, ShaderBuffer, StringLikeHasher,
                     std::equal_to<>>
      constant_buffers{};
  std::unordered_map<std::string, ShaderResourceDeclaration, StringLikeHasher,
                     std::equal_to<>>
      resources{};
};

} // namespace Reflection
