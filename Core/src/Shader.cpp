#include "pch/vkgpgpu_pch.hpp"

#include "Shader.hpp"

#include "Containers.hpp"
#include "Device.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <bit>
#include <execution>
#include <fstream>
#include <sstream>
#include <vulkan/vulkan.h>

#include "compilation/ShaderCompiler.hpp"
#include "reflection/ReflectionData.hpp"
#include "reflection/Reflector.hpp"

namespace std {
template <std::integral T> struct hash<vector<T>> {
  const size_t chunk_size = 10;

  size_t operator()(const vector<T> &v) const {

    vector<size_t> chunk_hashes((v.size() + chunk_size - 1) / chunk_size, 0);

    // Hash each chunk in parallel.
    std::for_each(std::execution::par, chunk_hashes.begin(), chunk_hashes.end(),
                  [&](size_t &chunk_hash) {
                    const size_t chunk_index = &chunk_hash - &chunk_hashes[0];
                    const size_t start_index = chunk_index * chunk_size;
                    const size_t end_index =
                        std::min(start_index + chunk_size, v.size());

                    for (size_t i = start_index; i < end_index; ++i) {
                      // Simple hash combination within each chunk.
                      chunk_hash ^= std::hash<T>{}(v[i]) + 0x9e3779b9 +
                                    (chunk_hash << 6) + (chunk_hash >> 2);
                    }
                  });

    // Combine chunk hashes in a final step.
    return std::accumulate(chunk_hashes.begin(), chunk_hashes.end(), size_t(0),
                           [](size_t acc, size_t h) {
                             return acc ^=
                                    h + 0x9e3779b9 + (acc << 6) + (acc >> 2);
                           });
  }
};
} // namespace std

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

Shader::Shader(const Device &dev,
               std::unordered_map<Type, std::vector<u32>> spirv_stages,
               std::string_view input_name)
    : device(dev) {
  for (const auto &[type, spirv] : spirv_stages) {
    parsed_spirv_per_stage_u32[type] = spirv;
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize =
        parsed_spirv_per_stage_u32[type].size() * sizeof(u32);
    create_info.pCode = parsed_spirv_per_stage_u32[type].data();

    verify(vkCreateShaderModule(device.get_device(), &create_info, nullptr,
                                &shader_modules[type]),
           "vkCreateShaderModule", "Failed to create shader module");
  }
  std::stringstream name_stream;
  name_stream << input_name;
  name = name_stream.str();
  const Reflection::Reflector reflector{*this};
  reflector.reflect(descriptor_set_layouts, reflection_data);
  create_descriptor_set_layouts();

  static constexpr std::hash<std::string> string_hasher;
  static constexpr std::hash<std::vector<u32>> vector_hasher{
      .chunk_size = 16,
  };
  auto name_hash = string_hasher(name);
  if (parsed_spirv_per_stage_u32.contains(Type::Compute)) {
    name_hash ^= vector_hasher(parsed_spirv_per_stage_u32.at(Type::Compute));
  }
  if (parsed_spirv_per_stage_u32.contains(Type::Vertex)) {
    name_hash ^= vector_hasher(parsed_spirv_per_stage_u32.at(Type::Vertex));
  }
  if (parsed_spirv_per_stage_u32.contains(Type::Fragment)) {
    name_hash ^= vector_hasher(parsed_spirv_per_stage_u32.at(Type::Fragment));
  }

  hash_value = name_hash;
}

Shader::Shader(
    const Device &dev,
    const std::unordered_set<PathShaderType, Hasher, std::equal_to<>> &types)
    : device(dev) {
  std::stringstream name_stream;
  if (types.size() > 1) {
    name_stream << "Combined-";
  }
  for (auto it = types.begin(); it != types.end(); ++it) {
    const auto &[path, type] = *it;
    parsed_spirv_per_stage[type] = read_file(path);
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = parsed_spirv_per_stage[type].size();
    create_info.pCode =
        std::bit_cast<const u32 *>(parsed_spirv_per_stage[type].data());

    verify(vkCreateShaderModule(device.get_device(), &create_info, nullptr,
                                &shader_modules[type]),
           "vkCreateShaderModule", "Failed to create shader module");

    name_stream << path.stem().string();
    if (std::next(it) != types.end()) {
      name_stream << "-";
    }
  }
  name = name_stream.str();
  const Reflection::Reflector reflector{*this};
  reflector.reflect(descriptor_set_layouts, reflection_data);
  create_descriptor_set_layouts();

  static constexpr std::hash<std::string> string_hasher;
  auto name_hash = string_hasher(name);
  if (parsed_spirv_per_stage.contains(Type::Compute)) {
    name_hash ^= string_hasher(parsed_spirv_per_stage.at(Type::Compute));
  }
  if (parsed_spirv_per_stage.contains(Type::Vertex)) {
    name_hash ^= string_hasher(parsed_spirv_per_stage.at(Type::Vertex));
  }
  if (parsed_spirv_per_stage.contains(Type::Fragment)) {
    name_hash ^= string_hasher(parsed_spirv_per_stage.at(Type::Fragment));
  }

  hash_value = name_hash;
}

Shader::~Shader() {
  for (const auto &[type, shader_module] : shader_modules) {
    vkDestroyShaderModule(device.get_device(), shader_module, nullptr);
  }
  for (const auto &layout : descriptor_set_layouts) {
    vkDestroyDescriptorSetLayout(device.get_device(), layout, nullptr);
  }
  debug("Destroyed Shader '{}'", name);
}

auto Shader::hash() const -> usize { return hash_value; }

auto Shader::has_descriptor_set(u32 set) const -> bool {
  return set < descriptor_set_layouts.size() &&
         descriptor_set_layouts[set] != nullptr;
}

auto Shader::get_descriptor_set(std::string_view descriptor_name, u32 set) const
    -> const VkWriteDescriptorSet * {
  if (set >= reflection_data.shader_descriptor_sets.size()) {
    return nullptr;
  }

  const auto &shader_descriptor_set =
      reflection_data.shader_descriptor_sets[set];
  if (shader_descriptor_set.write_descriptor_sets.contains(descriptor_name)) {
    const auto as_string = std::string{descriptor_name};
    return &shader_descriptor_set.write_descriptor_sets.at(as_string);
  }

  warn("Shader {0} does not contain requested descriptor set {1}", name,
       descriptor_name);
  return nullptr;
}

auto Shader::allocate_descriptor_set(u32 set) const
    -> Reflection::MaterialDescriptorSet {

  Reflection::MaterialDescriptorSet result;

  if (reflection_data.shader_descriptor_sets.empty()) {
    return result;
  }

  VkDescriptorSetAllocateInfo allocation_info = {};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorSetCount = 1;
  allocation_info.pSetLayouts = &descriptor_set_layouts[set];
  auto &allocated_set = result.descriptor_sets.emplace_back();
  allocated_set = device.get_descriptor_resource()->allocate_descriptor_set(
      allocation_info);
  return result;
}

auto Shader::create_descriptor_set_layouts() -> void {
  auto *vk_device = device.get_device();
  auto &descriptor_sets = reflection_data.shader_descriptor_sets;

  for (u32 set = 0; set < descriptor_sets.size(); set++) {
    auto &shader_descriptor_set = descriptor_sets[set];

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings{};
    for (const auto &[binding, uniform_buffer] :
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

    for (const auto &[binding, storage_buffer] :
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

    for (const auto &[binding, image_sampler] :
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

    for (const auto &[binding, image_sampler] :
         shader_descriptor_set.separate_textures) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

    for (const auto &[binding, image_sampler] :
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

    for (const auto &[binding_and_set, image_sampler] :
         shader_descriptor_set.storage_images) {
      auto &layout_binding = layout_bindings.emplace_back();
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      layout_binding.descriptorCount = image_sampler.array_size;
      layout_binding.stageFlags = image_sampler.shader_stage;
      layout_binding.pImmutableSamplers = nullptr;

      // Name a variable which has the value 0xFFFFFFFF
      static constexpr auto max_set = std::numeric_limits<Core::u32>::max();

      Core::u32 binding = binding_and_set & max_set;
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
          shader_descriptor_set.write_descriptor_sets[image_sampler.name];
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

    trace("Shader {0}: Creating descriptor set ['{1}'] with {2} ubo's, {3} "
          "ssbo's, "
          "{4} samplers, {5} separate textures, {6} separate samplers and {7} "
          "storage images.",
          name, set, shader_descriptor_set.uniform_buffers.size(),
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

auto to_shader_type(const std::filesystem::path &path) {
  Shader::Type from_extension;
  // Path can end in .spv, since it is compiled. The naming scheme is
  // filename.vert.spv, filename.frag.spv or filename.comp.spv
  if (path.extension() != ".spv") {
    error("Shader file {} does not have the .spv extension!", path.string());
    throw FileCouldNotBeOpened("Shader file " + path.string() +
                               " does not have the .spv extension!");
  }

  auto remove_extension = path;
  remove_extension.replace_extension();

  if (remove_extension.extension() == ".vert") {
    from_extension = Shader::Type::Vertex;
  } else if (remove_extension.extension() == ".frag") {
    from_extension = Shader::Type::Fragment;
  } else if (remove_extension.extension() == ".comp") {
    from_extension = Shader::Type::Compute;
  } else {
    error("Shader file {} does not have the .vert, .frag or .comp extension!",
          path.string());
    throw FileCouldNotBeOpened(
        "Shader file " + path.string() +
        " does not have the .vert, .frag or .comp extension!");
  }

  return from_extension;
}

auto Shader::compile_graphics(const Device &device,
                              const std::filesystem::path &vertex_path,
                              const std::filesystem::path &fragment_path)
    -> Ref<Shader> {
  ensure(compiler != nullptr, "ShaderCompiler is not initialized!");
  return compiler->compile_graphics(vertex_path, fragment_path);
}

auto Shader::compile_compute(const Device &device,
                             const std::filesystem::path &compute_path)
    -> Ref<Shader> {
  ensure(compiler != nullptr, "ShaderCompiler is not initialized!");
  return compiler->compile_compute(compute_path);
}

auto Shader::compile_graphics_scoped(const Device &device,
                                     const std::filesystem::path &vertex_path,
                                     const std::filesystem::path &fragment_path)
    -> Scope<Shader> {
  ensure(compiler != nullptr, "ShaderCompiler is not initialized!");
  return compiler->compile_graphics_scoped(vertex_path, fragment_path);
}

auto Shader::compile_compute_scoped(const Device &device,
                                    const std::filesystem::path &compute_path)
    -> Scope<Shader> {
  ensure(compiler != nullptr, "ShaderCompiler is not initialized!");
  return compiler->compile_compute_scoped(compute_path);
}

auto Shader::initialise_compiler(
    const Device &device, const Compilation::ShaderCompilerConfiguration &conf)
    -> void {
  compiler = make_scope<Compilation::ShaderCompiler>(device, conf);
}

} // namespace Core
