#include "pch/vkgpgpu_pch.hpp"

#include "Window.hpp"

#include "Verify.hpp"

#include <imgui_impl_glfw.h>

namespace Core {

Window::Window(const Instance &inst, const WindowProperties &props)
    : instance(&inst), properties(props) {
  if (!glfwInit()) {
    error("Failed to initialize GLFW");
    throw std::runtime_error("Failed to initialize GLFW");
  }

  if (!glfwVulkanSupported()) {
    error("Vulkan not supported");
    throw std::runtime_error("Vulkan not supported");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  auto &&[width, height] = properties.extent.as<i32>();

  window = glfwCreateWindow(width, height, "VkGPGPU", nullptr, nullptr);

  verify(glfwCreateWindowSurface(instance->get_instance(), window, nullptr,
                                 &surface),
         "glfwCreateWindowSurface", "Failed to create window surface");

  if (!properties.headless) {
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, i32 w, i32 h) {
      auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
      self.properties.extent = {static_cast<u32>(w), static_cast<u32>(h)};
    });
    glfwSetKeyCallback(window, [](GLFWwindow *win, i32 key, i32 scancode,
                                  i32 action, i32 mods) {
      auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
      if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        info("Escape key pressed, closing window");
        glfwSetWindowShouldClose(win, GLFW_TRUE);
      }
    });
  }
}

auto Window::should_close() const -> bool {
  if (window == nullptr)
    return false;

  return glfwWindowShouldClose(window);
}

auto Window::update() -> void { glfwPollEvents(); }

auto Window::get_native() const -> const GLFWwindow * { return window; }

auto Window::get_surface() const -> VkSurfaceKHR { return surface; }

auto Window::construct(const Instance &instance,
                       const WindowProperties &properties) -> Scope<Window> {
  return Scope<Window>{new Window{instance, properties}};
}

Window::~Window() {
  glfwDestroyWindow(window);
  glfwTerminate();
  vkDestroySurfaceKHR(instance->get_instance(), surface, nullptr);
}

auto Window::get_instance() const -> VkInstance {
  return instance->get_instance();
}

} // namespace Core
