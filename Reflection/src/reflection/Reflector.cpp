#include "reflection/Reflector.hpp"

#include <SPIRV-Cross/spirv_cross.hpp>
#include <SPIRV-Cross/spirv_glsl.hpp>

#include "Logger.hpp"
#include "Shader.hpp"
#include "Verify.hpp"

#include <array>
#include <vulkan/vulkan_core.h>

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

auto Reflector::reflect(std::vector<VkDescriptorSetLayout> &output) -> void {
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
  const auto k = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  const std::unordered_map<
      VkDescriptorType, const spirv_cross::SmallVector<spirv_cross::Resource> &>
      all_resource_small_vectors = {
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.uniform_buffers},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, resources.storage_buffers},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, resources.separate_images},
          {VK_DESCRIPTOR_TYPE_SAMPLER, resources.separate_samplers},
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resources.storage_images},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, resources.sampled_images},
          {VK_DESCRIPTOR_TYPE_MAX_ENUM, resources.stage_inputs},
          {VK_DESCRIPTOR_TYPE_MAX_ENUM, resources.stage_outputs}};

  for (const auto &small_vector : all_resource_small_vectors) {
    count_max_pass(*impl->compiler, small_vector);
  }

  for (auto &&[k, _] : resources_by_set) {
    info("Set {}", k);
  }

  // Second pass is the actual reflection
  auto reflect_pass = [&set_resources = resources_by_set,
                       &output](const auto &compiler, const auto &resources,
                                auto &layouts) {
    for (const auto &resource : resources.second) {
      const auto set =
          compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

      const auto binding =
          compiler.get_decoration(resource.id, spv::DecorationBinding);

      const auto &type = compiler.get_type(resource.type_id);
      const auto &type_name = compiler.get_name(type.self);

      info("Resource {} is in set {} and binding {} of type {}", resource.name,
           set, binding, type_name);

      // Create a descriptor set layout if it doesn't exist
      if (!layouts.contains(set)) {
        layouts[set] = {};
      }

      // Create a descriptor set layout binding
      VkDescriptorSetLayoutBinding layout_binding{};
      layout_binding.binding = binding;
      layout_binding.descriptorType = resources.first;
      layout_binding.descriptorCount = 1;
      layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

      // Add the descriptor set layout binding to the descriptor set layout
      layouts[set].push_back(layout_binding);
    }
  };

  for (const auto &small_vector : all_resource_small_vectors) {
    reflect_pass(*impl->compiler, small_vector, set_bindings);
  }

  // Create the descriptor set layouts
  for (auto &&[set, bindings] : set_bindings) {
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = bindings.size();
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