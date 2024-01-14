#pragma once

#include "Types.hpp"

#include <GLFW/glfw3.h>
#include <ImageProperties.hpp>

namespace Core {

struct WindowProperties {
  bool headless{true}; // Whether or not to create a window
  Extent<u32> extent{800, 600};
  std::string title{"VkGPGPU"};
};

class Window {
public:
  explicit Window(const WindowProperties &);
  ~Window() = default;

  [[nodiscard]] auto raw() const -> GLFWwindow * { return window.get(); }
  [[nodiscard]] auto should_close() const -> bool;

private:
  struct Deleter {
    auto operator()(GLFWwindow *window) const -> void {
      glfwDestroyWindow(window);
    }
  };
  Scope<GLFWwindow, Deleter> window{nullptr};
};

} // namespace Core
