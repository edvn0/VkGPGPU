#pragma once

#include "Ensure.hpp"
#include "Exception.hpp"
#include "Logger.hpp"

#include <cassert> // for assert (or your preferred debug break method)
#include <fmt/core.h>
#include <vulkan/vulkan.h>

namespace Core {

class VulkanResultException : public BaseException {
public:
  VulkanResultException(VkResult result, const std::string &message)
      : BaseException(message), vulkan_result(result) {}

  [[nodiscard]] auto get_result() const { return vulkan_result; }

private:
  VkResult vulkan_result;
};

auto vk_result_to_string(VkResult result) -> std::string;

template <typename... Args>
void verify(VkResult result, const std::string &function_name,
            fmt::format_string<Args...> format, Args &&...args) {
  if (result != VK_SUCCESS) {
    std::string formatted_message =
        fmt::format(format, std::forward<Args>(args)...);
    std::string error_message = function_name + " failed with VkResult: " +
                                vk_result_to_string(result) + ", " +
                                formatted_message;
    error("{}", error_message);
    throw VulkanResultException(result, error_message);
  }
}

} // namespace Core
