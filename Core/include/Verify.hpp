#pragma once

#include "Exception.hpp"
#include "Logger.hpp"
#include <fmt/core.h>
#include <vulkan/vulkan.h>

namespace Core {

class VulkanResultException : public BaseException {
public:
  VulkanResultException(VkResult result, const std::string &message)
      : BaseException(message), result_(result) {}

  VkResult getResult() const { return result_; }

private:
  VkResult result_;
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
  } else {
    debug("{} succeeded", function_name);
  }
}

template <auto T> [[noreturn]] auto unreachable_return() -> decltype(T) {
  throw BaseException{"Invalidly here!"};
}

} // namespace Core