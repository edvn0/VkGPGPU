#pragma once

#include "Device.hpp"
#include "Event.hpp"
#include "Types.hpp"

#include <functional>

extern "C" {
struct GLFWwindow;
}

namespace Core {

struct WindowProperties {
  Extent<u32> extent{1280, 720};
  bool fullscreen{false};
  bool vsync{false};
  bool headless{false};
};

class Window {
public:
  virtual ~Window();
  auto update() -> void;
  [[nodiscard]] auto get_native() const -> const GLFWwindow *;
  [[nodiscard]] auto get_native() -> GLFWwindow *;
  [[nodiscard]] auto get_surface() const -> VkSurfaceKHR;
  [[nodiscard]] auto should_close() const -> bool;

  [[nodiscard]] auto was_resized() -> bool;
  auto reset_resize_status() -> void;

  [[nodiscard]] auto size_is_zero() const -> bool;
  [[nodiscard]] auto get_extent() const -> Extent<u32>;
  [[nodiscard]] auto get_framebuffer_size() const -> Extent<u32>;
  [[nodiscard]] auto get_properties() const -> const WindowProperties &;

  static auto construct(const Instance &, const WindowProperties &)
      -> Scope<Window>;

  [[nodiscard]] auto get_instance() const -> VkInstance;

protected:
  Window(const Instance &, const WindowProperties &);

private:
  const Instance *instance{};

  WindowProperties properties;
  GLFWwindow *window{nullptr};
  VkSurfaceKHR surface{};

  struct UserPointer {
    bool was_resized{false};
    std::function<void(Event &)> event_callback{};
  };
  UserPointer user_data{};
};

} // namespace Core
