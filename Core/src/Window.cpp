#include "pch/vkgpgpu_pch.hpp"

#include "Window.hpp"

#include "Verify.hpp"

#include <imgui_impl_glfw.h>

namespace Core {

Window::Window(const Instance &inst, const WindowProperties &props)
    : instance(&inst), properties(props) {
  if (properties.headless)
    return;

  if (!glfwInit()) {
    error("Failed to initialize GLFW");
    throw std::runtime_error("Failed to initialize GLFW");
  }

  if (!glfwVulkanSupported()) {
    error("Vulkan not supported");
    throw std::runtime_error("Vulkan not supported");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  auto &&[width, height] = properties.extent.as<i32>();

  window = glfwCreateWindow(width, height, "VkGPGPU", nullptr, nullptr);

  verify(glfwCreateWindowSurface(instance->get_instance(), window, nullptr,
                                 &surface),
         "glfwCreateWindowSurface", "Failed to create window surface");

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(
      window, +[](GLFWwindow *win, i32 w, i32 h) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        self.properties.extent = {
            static_cast<u32>(w),
            static_cast<u32>(h),
        };
        self.user_data.was_resized = true;
        info("Window framebuffer resized to {}x{}", w, h);
      });
  glfwSetWindowSizeCallback(
      window, +[](GLFWwindow *win, i32 w, i32 h) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        self.properties.extent = {
            static_cast<u32>(w),
            static_cast<u32>(h),
        };
        self.user_data.was_resized = true;
        info("Window resized to {}x{}", w, h);
      });
  glfwSetKeyCallback(
      window,
      +[](GLFWwindow *win, i32 key, i32 scancode, i32 action, i32 mods) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
          info("Escape key pressed, closing window");
          glfwSetWindowShouldClose(win, GLFW_TRUE);
        }
      });
  glfwSetMouseButtonCallback(
      window, +[](GLFWwindow *win, i32 button, i32 action, i32 mods) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
          info("Left mouse button pressed");
        }
      });
  glfwSetCursorPosCallback(
      window, +[](GLFWwindow *win, double x, double y) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        info("Mouse moved to {}, {}", x, y);
      });
}

auto Window::should_close() const -> bool {
  if (window == nullptr)
    return false;

  return glfwWindowShouldClose(window) != GLFW_FALSE;
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

auto Window::resized_or_minimized() -> bool {
  if (window == nullptr) {
    return false;
  }

  return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0 ||
         user_data.was_resized;
}

auto Window::reset_resize_status() -> void { user_data.was_resized = false; }

auto Window::size_is_zero() const -> bool {
  auto &&[width, height] = get_framebuffer_size();
  return width == 0 || height == 0;
}

auto Window::get_extent() const -> Extent<u32> { return properties.extent; }

auto Window::get_framebuffer_size() const -> Extent<u32> {
  i32 w{};
  i32 h{};
  glfwGetFramebufferSize(window, &w, &h);
  return {
      static_cast<u32>(w),
      static_cast<u32>(h),
  };
}

auto Window::get_properties() const -> const WindowProperties & {
  return properties;
}

} // namespace Core
