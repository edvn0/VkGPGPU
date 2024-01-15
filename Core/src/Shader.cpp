#include "pch/vkgpgpu_pch.hpp"

#include "Shader.hpp"

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

  Reflection::Reflector reflector{*this};
  reflector.reflect(descriptor_set_layouts, reflection_data);
}

Shader::~Shader() {
  vkDestroyShaderModule(device.get_device(), shader_module, nullptr);
}

} // namespace Core
