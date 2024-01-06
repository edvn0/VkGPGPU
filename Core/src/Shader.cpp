#include "Shader.hpp"
#include "Device.hpp"
#include "Exception.hpp"
#include "Verify.hpp"
#include <bit>
#include <fstream>
#include <vulkan/vulkan.h>

namespace Core {

class FileCouldNotBeOpened : public BaseException {
public:
  using BaseException::BaseException;
};

auto read_file(const std::filesystem::path &path) -> std::string {
  const auto resolved = std::filesystem::absolute(path);
  std::ifstream stream{resolved, std::ios::binary};
  if (!stream) {
    throw FileCouldNotBeOpened{"Could not open file: " +
                               std::filesystem::absolute(resolved).string()};
  }

  return std::string{std::istreambuf_iterator<char>{stream},
                     std::istreambuf_iterator<char>{}};
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