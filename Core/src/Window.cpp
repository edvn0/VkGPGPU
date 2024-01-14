#include "Window.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Logger.hpp"

#include <GLFW/glfw3.h>

namespace Core {

Window::Window(const WindowProperties &props) {
  if (props.headless) {
    info("Headless mode enabled, not creating window!");
    return;
  }

  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW!");
  }

  if (!glfwVulkanSupported()) {
    throw std::runtime_error("Vulkan is not supported!");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  const auto width = static_cast<i32>(props.extent.width);
  const auto height = static_cast<i32>(props.extent.width);
  window = Scope<GLFWwindow, Deleter>{
      glfwCreateWindow(width, height, props.title.c_str(), nullptr, nullptr)};
}

auto Window::should_close() const -> bool {
  if (window) {
    return glfwWindowShouldClose(window.get());
  }
  // In headless mode, we never want to close the window
  return false;
}

} // namespace Core
