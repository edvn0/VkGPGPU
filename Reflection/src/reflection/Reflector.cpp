#include "reflection/Reflector.hpp"

#include "Exception.hpp"
#include "Logger.hpp"
#include "Shader.hpp"
#include "Verify.hpp"

#include <SPIRV-Cross/spirv_cross.hpp>
#include <SPIRV-Cross/spirv_glsl.hpp>
#include <array>
#include <ranges>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "reflection/ReflectionData.hpp"

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace Reflection {

struct Reflector::CompilerImpl {
  Core::Scope<spirv_cross::Compiler> compiler;
  spirv_cross::ShaderResources shader_resources;

  CompilerImpl(Core::Scope<spirv_cross::Compiler> &&compiler,
               spirv_cross::ShaderResources &&shader_resources)
      : compiler(std::move(compiler)),
        shader_resources(std::move(shader_resources)) {}
};

Reflector::Reflector(Core::Shader &shader) : shader(shader) {
  const std::string &data = shader.get_code();
  const auto size = data.size();
  const auto *spirv_data = std::bit_cast<const Core::u32 *>(data.data());

  // Does size need to be changed after the cast?
  const auto fixed_size = size / sizeof(Core::u32);

  auto compiler =
      Core::make_scope<spirv_cross::Compiler>(spirv_data, fixed_size);
  auto shader_resources = compiler->get_shader_resources();

  impl = Core::make_scope<Reflector::CompilerImpl>(std::move(compiler),
                                                   std::move(shader_resources));
}

Reflector::~Reflector() = default;

static constexpr auto check_for_gaps = [](const auto &unordered_map) {
  if (unordered_map.empty())
    return false;

  std::int32_t max_index = 0;
  for (const auto &key : unordered_map | std::views::keys) {
    max_index = std::max(max_index, static_cast<Core::i32>(key));
  }

  for (int i = 0; i < max_index; ++i) {
    if (!unordered_map.contains(i)) {
      return true;
    }
  }

  return false;
};

namespace Detail {

std::unordered_map<std::uint32_t,
                   std::unordered_map<std::uint32_t, Reflection::UniformBuffer>>
    global_uniform_buffers{};
std::unordered_map<std::uint32_t,
                   std::unordered_map<std::uint32_t, Reflection::StorageBuffer>>
    global_storage_buffers{};

template <VkDescriptorType Type> struct DescriptorTypeReflector {
  static auto
  reflect(const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &reflection_data) -> void {
    throw Core::BaseException(
        fmt::format("Unknown descriptor type: {}", typeid(Type).name()));
  }
};

template <> struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      if (auto active_buffers = compiler.get_active_buffer_ranges(resource.id);
          active_buffers.empty()) {
        continue;
      }

      const auto &name = resource.name;
      const auto &buffer_type = compiler.get_type(resource.base_type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto size = static_cast<std::uint32_t>(
          compiler.get_declared_struct_size(buffer_type));

      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      if (!global_uniform_buffers[descriptor_set].contains(binding)) {
        Reflection::UniformBuffer uniform_buffer;
        uniform_buffer.binding_point = binding;
        uniform_buffer.size = size;
        uniform_buffer.name = name;
        uniform_buffer.shader_stage = VK_SHADER_STAGE_ALL_GRAPHICS;
        global_uniform_buffers.at(descriptor_set)[binding] = uniform_buffer;
      } else {
        if (auto &uniform_buffer =
                global_uniform_buffers.at(descriptor_set).at(binding);
            size > uniform_buffer.size) {
          uniform_buffer.size = size;
        }
      }
      shader_descriptor_set.uniform_buffers[binding] =
          global_uniform_buffers.at(descriptor_set).at(binding);
    }
  }
};

template <> struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      if (auto active_buffers = compiler.get_active_buffer_ranges(resource.id);
          active_buffers.empty()) {
        continue;
      }

      const auto &name = resource.name;
      const auto &buffer_type = compiler.get_type(resource.base_type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto size = static_cast<std::uint32_t>(
          compiler.get_declared_struct_size(buffer_type));

      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      if (!global_storage_buffers[descriptor_set].contains(binding)) {
        Reflection::StorageBuffer storage_buffer;
        storage_buffer.binding_point = binding;
        storage_buffer.size = size;
        storage_buffer.name = name;
        storage_buffer.shader_stage = VK_SHADER_STAGE_ALL_GRAPHICS;
        global_storage_buffers.at(descriptor_set)[binding] = storage_buffer;
      } else {
        if (auto &storage_buffer =
                global_storage_buffers.at(descriptor_set).at(binding);
            size > storage_buffer.size) {
          storage_buffer.size = size;
        }
      }

      shader_descriptor_set.storage_buffers[binding] =
          global_storage_buffers.at(descriptor_set).at(binding);
    }
  }
};

template <> struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      const auto &name = resource.name;
      const auto &type = compiler.get_type(resource.type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto array_size = type.array[0];
      if (array_size == 0 || array_size > 16) {
        array_size = 1;
      }
      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      auto &image_sampler = shader_descriptor_set.sampled_images[binding];
      image_sampler.binding_point = binding;
      image_sampler.descriptor_set = descriptor_set;
      image_sampler.name = name;
      image_sampler.shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      image_sampler.array_size = array_size;

      output.resources[name] =
          Reflection::ShaderResourceDeclaration(name, binding, 1);
    }
  }
};

template <> struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      const auto &name = resource.name;
      const auto &type = compiler.get_type(resource.type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto array_size = type.array[0];
      if (array_size == 0 || array_size > 16) {
        array_size = 1;
      }
      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      auto &image_sampler = shader_descriptor_set.storage_images[binding];
      image_sampler.binding_point = binding;
      image_sampler.descriptor_set = descriptor_set;
      image_sampler.name = name;
      image_sampler.array_size = array_size;
      image_sampler.shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT;

      output.resources[name] =
          Reflection::ShaderResourceDeclaration(name, binding, 1);
    }
  };
};

template <>
struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      const auto &name = resource.name;
      const auto &type = compiler.get_type(resource.type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto array_size = type.array[0];
      if (array_size == 0 || array_size > 16) {
        array_size = 1;
      }
      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      auto &image_sampler = shader_descriptor_set.separate_textures[binding];
      image_sampler.binding_point = binding;
      image_sampler.descriptor_set = descriptor_set;
      image_sampler.name = name;
      image_sampler.array_size = array_size;
      image_sampler.shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT;

      output.resources[name] =
          Reflection::ShaderResourceDeclaration(name, binding, 1);
    }
  };
};

template <> struct DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_SAMPLER> {
  static auto
  reflect(const spirv_cross::Compiler &compiler,
          const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          ReflectionData &output) -> void {
    for (const auto &resource : resources) {
      const auto &name = resource.name;
      const auto &type = compiler.get_type(resource.type_id);
      auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto descriptor_set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
      auto array_size = type.array[0];
      if (array_size == 0 || array_size > 16) {
        array_size = 1;
      }
      if (descriptor_set >= output.shader_descriptor_sets.size()) {
        output.shader_descriptor_sets.resize(descriptor_set + 1);
      }

      auto &shader_descriptor_set =
          output.shader_descriptor_sets[descriptor_set];
      auto &image_sampler = shader_descriptor_set.separate_samplers[binding];
      image_sampler.binding_point = binding;
      image_sampler.descriptor_set = descriptor_set;
      image_sampler.name = name;
      image_sampler.array_size = array_size;
      image_sampler.shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT;

      output.resources[name] =
          Reflection::ShaderResourceDeclaration(name, binding, 1);
    }
  };
};

} // namespace Detail

auto reflect_on_resource(
    const auto &compiler, VkDescriptorType type,
    const spirv_cross::SmallVector<spirv_cross::Resource> &resources,
    ReflectionData &reflection_data) -> void {
  switch (type) {
  case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    Detail::DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>::reflect(
        compiler, resources, reflection_data);
    break;
  case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    Detail::DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>::reflect(
        compiler, resources, reflection_data);
    break;
  case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    Detail::DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>::reflect(
        compiler, resources, reflection_data);
    break;
  case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    Detail::DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>::reflect(
        compiler, resources, reflection_data);
    break;
  case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    Detail::DescriptorTypeReflector<
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER>::reflect(compiler, resources,
                                                            reflection_data);
    break;
  case VK_DESCRIPTOR_TYPE_SAMPLER:
    Detail::DescriptorTypeReflector<VK_DESCRIPTOR_TYPE_SAMPLER>::reflect(
        compiler, resources, reflection_data);
    break;
  default:
    error("Unknown descriptor type: {}", (Core::u32)type);
    throw std::runtime_error("Unknown descriptor type");
  }
}

auto Reflector::reflect(std::vector<VkDescriptorSetLayout> &output,
                        ReflectionData &reflection_data_output) -> void {
  // This may be crazy, but I'm going to clear the output vector
  output.clear();

  // Show all the resources in the shader
  // How many sets are there? I.e. how many descriptor set layouts do we need?
  const auto &resources = impl->shader_resources;

  std::unordered_map<Core::u32, std::vector<VkDescriptorSetLayoutBinding>>
      set_bindings;
  std::unordered_map<Core::u32, std::vector<spirv_cross::Resource>>
      resources_by_set;
  // First pass is just count the max set number
  auto count_max_pass =
      [&set_resources = resources_by_set](
          const auto &compiler,
          std::pair<VkDescriptorType,
                    const spirv_cross::SmallVector<spirv_cross::Resource> &>
              resources) {
        for (const auto &resource : resources.second) {
          const auto set = compiler.get_decoration(
              resource.id, spv::DecorationDescriptorSet);

          if (set_resources.contains(set)) {
            set_resources.at(set).push_back(resource);
          } else {
            set_resources[set] = {resource};
          }
        }
      };
  const std::unordered_map<
      VkDescriptorType, const spirv_cross::SmallVector<spirv_cross::Resource> &>
      all_resource_small_vectors = {
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.uniform_buffers},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, resources.storage_buffers},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, resources.separate_images},
          {VK_DESCRIPTOR_TYPE_SAMPLER, resources.separate_samplers},
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resources.storage_images},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, resources.sampled_images},
      };

  for (const auto &small_vector : all_resource_small_vectors) {
    count_max_pass(*impl->compiler, small_vector);
  }

  if (check_for_gaps(resources_by_set)) {
    error("There are gaps in the descriptor sets for shader: {}",
          shader.get_name());
    throw Core::BaseException("There are gaps in the descriptor sets");
  }

  // Second pass, reflect
  for (const auto &[k, v] : all_resource_small_vectors) {
    reflect_on_resource(*impl->compiler, k, v, reflection_data_output);
  }

  // Third pass, create the descriptor set layouts
  for (const auto &bindings : set_bindings | std::views::values) {
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = static_cast<Core::u32>(bindings.size());
    create_info.pBindings = bindings.data();

    VkDescriptorSetLayout &layout = output.emplace_back();
    Core::verify(vkCreateDescriptorSetLayout(shader.get_device().get_device(),
                                             &create_info, nullptr, &layout),
                 "vkCreateDescriptorSetLayout",
                 "Failed to create descriptor set layout for shader: {}",
                 shader.get_name());
  }

  info("Created {} descriptor set layouts for shader: {}", output.size(),
       shader.get_name());
}

} // namespace Reflection
