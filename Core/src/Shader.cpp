#include "pch/vkgpgpu_pch.hpp"

#include "Device.hpp"
#include "Exception.hpp"
#include "Shader.hpp"
#include "Verify.hpp"
#include <bit>
#include <fstream>
#include <vulkan/vulkan.h>


namespace Core {

class FileCouldNotBeOpened : public BaseException {
public:
  using BaseException::BaseException;
};

auto read_file(const std::filesystem::path& path) -> std::string {
  // Convert to an absolute path and open the file
  const auto& absolute_path = absolute(path);
  std::ifstream file(absolute_path);
  // Set exceptions on
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  // Check if the file was successfully opened
  if (!file.is_open()) {
    throw FileCouldNotBeOpened("Failed to open file: " + absolute_path.string());
  }

  // Use a std::stringstream to read the file's contents into a string
  std::stringstream buffer;
  buffer << file.rdbuf();

  // Return the contents of the file as a string
  return buffer.str();
}

Shader::Shader(const std::filesystem::path &path) {
  auto shader_code = read_file(path);

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_code.size();
  create_info.pCode = std::bit_cast<const u32 *>(shader_code.data());

  const auto device = Device::get()->get_device();

  verify(vkCreateShaderModule(device, &create_info, nullptr, &shader_module),
         "vkCreateShaderModule", "Failed to create shader module");
}

Shader::~Shader() {
  auto device = Device::get()->get_device();
  vkDestroyShaderModule(device, shader_module, nullptr);
}

} // namespace Core