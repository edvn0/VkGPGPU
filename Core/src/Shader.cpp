#include "pch/vkgpgpu_pch.hpp"

#include "Shader.hpp"

#include "Containers.hpp"
#include "Device.hpp"
#include "Exception.hpp"
#include "Verify.hpp"

#include <bit>
#include <fstream>
#include <vulkan/vulkan.h>

#include "reflection/Reflector.hpp"

namespace Core {

class FileCouldNotBeOpened : public BaseException {
public:
  using BaseException::BaseException;
};

auto read_file(const std::filesystem::path &path) -> std::string {
  // Convert to an absolute path and open the file
  const auto &absolute_path = absolute(path);
  const std::ifstream file(absolute_path, std::ios::in | std::ios::binary);

  // Check if the file was successfully opened
  if (!file.is_open()) {
    error("Failed to open file: {}", absolute_path.string());
    throw FileCouldNotBeOpened("Failed to open file: " +
                               absolute_path.string());
  }

  // Use a std::stringstream to read the file's contents into a string
  std::stringstream buffer;
  buffer << file.rdbuf();

  // Return the contents of the file as a string
  return buffer.str();
}

Shader::Shader(const Device &dev, const std::filesystem::path &path)
    : device(dev), name(path.stem().string()) {
  parsed_spirv_per_stage[Type::Compute] = read_file(path);
  const auto &shader_code = parsed_spirv_per_stage.at(Type::Compute);
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_code.size();
  create_info.pCode = std::bit_cast<const u32 *>(shader_code.data());

  verify(vkCreateShaderModule(device.get_device(), &create_info, nullptr,
                              &shader_module),
         "vkCreateShaderModule", "Failed to create shader module");

  const Reflection::Reflector reflector{*this};
  reflector.reflect(descriptor_set_layouts, reflection_data);
  create_descriptor_set_layouts();
  create_push_constant_ranges();
}

Shader::~Shader() {
  vkDestroyShaderModule(device.get_device(), shader_module, nullptr);
  info("Destroyed shader module!");
  for (const auto &descriptor_set_layout : descriptor_set_layouts) {
    vkDestroyDescriptorSetLayout(device.get_device(), descriptor_set_layout,
                                 nullptr);
  }
}

auto Shader::create_descriptor_set_layouts() -> void {
  auto *vk_device = device.get_device();

  for (u32 set = 0; set < reflection_data.shader_descriptor_sets.size();
       set++) {
    auto &shader_descriptor_set = reflection_data.shader_descriptor_sets[set];

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings{};
    for (auto &[binding, uniform_buffer] :
         shader_descriptor_set.uniform_buffers) {
      VkDescriptorSetLayoutBinding &layout_binding =
          layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      layout_binding.descriptorCount = 1;
      layout_binding.stageFlags = uniform_buffer.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;
      layout_binding.binding = binding;

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[uniform_buffer.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = 1;
      write_set.dstBinding = layout_binding.binding;
    }

    for (auto &[binding, storage_buffer] :
         shader_descriptor_set.storage_buffers) {
      VkDescriptorSetLayoutBinding &layout_binding =
          layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      layout_binding.descriptorCount = 1;
      layout_binding.stageFlags = storage_buffer.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;
      layout_binding.binding = binding;
      ensure(!shader_descriptor_set.uniform_buffers.contains(binding),
             "Binding is already present!");

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[storage_buffer.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = 1;
      write_set.dstBinding = layout_binding.binding;
    }

    for (auto &[binding, image_sampler] :
         shader_descriptor_set.sampled_images) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      layout_binding.descriptorCount = image_sampler.array_size;
      layout_binding.stageFlags = image_sampler.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;
      layout_binding.binding = binding;

      ensure(!shader_descriptor_set.uniform_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.storage_buffers.contains(binding),
             "Binding is already present!");

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[image_sampler.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = image_sampler.array_size;
      write_set.dstBinding = layout_binding.binding;
    }

    for (auto &[binding, image_sampler] :
         shader_descriptor_set.separate_textures) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      layout_binding.descriptorCount = image_sampler.array_size;
      layout_binding.stageFlags = image_sampler.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;
      layout_binding.binding = binding;

      ensure(!shader_descriptor_set.uniform_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.sampled_images.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.storage_buffers.contains(binding),
             "Binding is already present!");

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[image_sampler.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = image_sampler.array_size;
      write_set.dstBinding = layout_binding.binding;
    }

    for (auto &[binding, image_sampler] :
         shader_descriptor_set.separate_samplers) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      layout_binding.descriptorCount = image_sampler.array_size;
      layout_binding.stageFlags = image_sampler.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;
      layout_binding.binding = binding;

      ensure(!shader_descriptor_set.uniform_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.sampled_images.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.storage_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.separate_textures.contains(binding),
             "Binding is already present!");

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[image_sampler.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = image_sampler.array_size;
      write_set.dstBinding = layout_binding.binding;
    }

    for (auto &[bindingAndSet, imageSampler] :
         shader_descriptor_set.storage_images) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      layout_binding.descriptorCount = imageSampler.array_size;
      layout_binding.stageFlags = imageSampler.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;

      // Name a variable which has the value 0xFFFFFFFF
      static constexpr auto max_set = std::numeric_limits<Core::u32>::max();

      Core::u32 binding = bindingAndSet & max_set;
      // uint32_t descriptorSet = (bindingAndSet >> 32);
      layout_binding.binding = binding;

      ensure(!shader_descriptor_set.uniform_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.storage_buffers.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.sampled_images.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.separate_textures.contains(binding),
             "Binding is already present!");
      ensure(!shader_descriptor_set.separate_samplers.contains(binding),
             "Binding is already present!");

      VkWriteDescriptorSet &write_set =
          shader_descriptor_set.write_descriptor_sets[imageSampler.name];
      write_set = {};
      write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set.descriptorType = layout_binding.descriptorType;
      write_set.descriptorCount = 1;
      write_set.dstBinding = layout_binding.binding;
    }

    Container::sort(layout_bindings, [](const VkDescriptorSetLayoutBinding &a,
                                        const VkDescriptorSetLayoutBinding &b) {
      return a.binding < b.binding;
    });

    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = nullptr;
    descriptor_layout.bindingCount =
        static_cast<Core::u32>(layout_bindings.size());
    descriptor_layout.pBindings = layout_bindings.data();

    info("Shader: Creating descriptor set ['{0}'] with {1} ubo's, {2} ssbo's, "
         "{3} samplers, {4} separate textures, {5} separate samplers and {6} "
         "storage images.",
         set, shader_descriptor_set.uniform_buffers.size(),
         shader_descriptor_set.storage_buffers.size(),
         shader_descriptor_set.sampled_images.size(),
         shader_descriptor_set.separate_textures.size(),
         shader_descriptor_set.separate_samplers.size(),
         shader_descriptor_set.storage_images.size());
    if (set >= descriptor_set_layouts.size()) {
      descriptor_set_layouts.resize(static_cast<std::size_t>(set) + 1);
    }
    auto &current_layout_per_set = descriptor_set_layouts[set];
    verify(vkCreateDescriptorSetLayout(vk_device, &descriptor_layout, nullptr,
                                       &current_layout_per_set),
           "vkCreateDescriptorSetLayout",
           "Failed to create descriptor set layout");
  }
}

void Shader::create_push_constant_ranges() {
  const auto &pc_ranges = reflection_data.push_constant_ranges;
  for (const auto &[offset, size, shader_stage] : pc_ranges) {
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = shader_stage;
    push_constant_range.offset = offset;
    push_constant_range.size = size;
    push_constant_ranges.push_back(push_constant_range);
  }
}

} // namespace Core
