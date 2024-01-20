#pragma once

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

template <typename... Args>
void ensure(bool condition, fmt::format_string<Args...> message,
            Args &&...args) {
  if (!condition) {
    auto formatted_message = fmt::format(message, std::forward<Args>(args)...);
    // Log the formatted message, throw an exception, or handle the error as
    // needed For example, using assert to trigger a debug break
    error("{}", formatted_message);
    assert(false);
  }
}

template <auto T> [[noreturn]] auto unreachable_return() -> decltype(T) {
  throw BaseException{"Invalidly here!"};
}

} // namespace Core
