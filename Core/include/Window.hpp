#pragma once

#include "Device.hpp"
#include "Types.hpp"

#include <GLFW/glfw3.h>

namespace Core {

struct WindowProperties {
  Extent<u32> extent{1280, 720};
  bool fullscreen{false};
  bool vsync{false};
  bool headless{false};
};

class Window {
public:
  ~Window();
  auto update() -> void;
  [[nodiscard]] auto get_native() const -> const GLFWwindow *;
  [[nodiscard]] auto get_surface() const -> VkSurfaceKHR;
  [[nodiscard]] auto should_close() const -> bool;

  static auto construct(const Instance &, const WindowProperties &)
      -> Scope<Window>;

  auto get_instance() const -> VkInstance;

private:
  Window(const Instance &, const WindowProperties &);
  const Instance *instance{};

  WindowProperties properties;
  GLFWwindow *window{nullptr};
  VkSurfaceKHR surface{};
};

} // namespace Core
