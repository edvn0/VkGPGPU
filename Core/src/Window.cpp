#include "pch/vkgpgpu_pch.hpp"

#include "Window.hpp"

#include "Input.hpp"
#include "Verify.hpp"

#include <GLFW/glfw3.h>
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

  if (properties.begin_fullscreen) {
    // If starting in fullscreen, get the primary monitor and its video mode
    auto *monitor = glfwGetPrimaryMonitor();
    const auto *mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_DECORATED, false);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    // Create a fullscreen window using the primary monitor's resolution
    window = glfwCreateWindow(mode->width, mode->height, "VkGPGPU", monitor,
                              nullptr);
    properties.fullscreen = true; // Set the fullscreen property to true
  } else {
    // Create the window in windowed mode with the specified size
    auto &&[width, height] = properties.extent.as<i32>();
    window = glfwCreateWindow(width, height, "VkGPGPU", nullptr, nullptr);
  }
  Input::initialise(window);

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
      });

  glfwSetKeyCallback(
      window,
      +[](GLFWwindow *win, i32 key, i32 scancode, i32 action, i32 mods) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        // send different based on action, mods etc: KeyPressedEVent,
        // KeyReleasedEvent, KeyTypedEvent
        switch (action) {
        case GLFW_PRESS: {
          KeyPressedEvent event{key, 0};
          self.user_data.event_callback(event);
          break;
        }
        case GLFW_RELEASE: {
          KeyReleasedEvent event{key};
          self.user_data.event_callback(event);
          break;
        }
        case GLFW_REPEAT: {
          KeyPressedEvent event{key, 1};
          self.user_data.event_callback(event);
          break;
        }
        default: {
          error("Unknown key action: {}", action);
          break;
        }
        }
      });
  glfwSetWindowSizeCallback(
      window, +[](GLFWwindow *win, i32 w, i32 h) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        self.properties.extent = {
            static_cast<u32>(w),
            static_cast<u32>(h),
        };
        self.user_data.was_resized = true;

        WindowResizeEvent event{w, h};
        self.user_data.event_callback(event);
      });

  glfwSetScrollCallback(
      window, +[](GLFWwindow *win, f64 x, f64 y) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));
        MouseScrolledEvent event{static_cast<float>(x), static_cast<float>(y)};
        self.user_data.event_callback(event);
      });

  glfwSetMouseButtonCallback(
      window, +[](GLFWwindow *win, int button, int action, int mods) {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(win));

        // Assuming you have a way to get the current mouse position
        double mouse_x;
        double mouse_y;
        glfwGetCursorPos(win, &mouse_x, &mouse_y);

        if (action == GLFW_PRESS) {
          MouseButtonPressedEvent event(static_cast<MouseCode>(button),
                                        static_cast<floating>(mouse_x),
                                        static_cast<floating>(mouse_y));
          self.user_data.event_callback(event);
        }
        if (action == GLFW_RELEASE) {
          MouseButtonReleasedEvent event(static_cast<MouseCode>(button),
                                         static_cast<floating>(mouse_x),
                                         static_cast<floating>(mouse_y));
          self.user_data.event_callback(event);
        }
      });
}

auto Window::toggle_fullscreen() -> void {
  if (!properties.fullscreen) {
    // Store current window size and position
    i32 w{}, h{}, x{}, y{};
    glfwGetWindowSize(window, &w, &h);
    glfwGetWindowPos(window, &x, &y);

    properties.windowed_width = static_cast<u32>(w);
    properties.windowed_height = static_cast<u32>(h);
    properties.windowed_position_x = static_cast<u32>(x);
    properties.windowed_position_y = static_cast<u32>(y);

    // Get the primary monitor and its video mode
    auto *monitor = glfwGetPrimaryMonitor();
    const auto *mode = glfwGetVideoMode(monitor);

    // Switch to fullscreen
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                         mode->refreshRate);
    properties.fullscreen = true;
    user_data.was_resized = true;
  } else {
    // Switch back to windowed mode
    glfwSetWindowMonitor(window, nullptr, properties.windowed_position_x,
                         properties.windowed_position_y,
                         properties.windowed_width, properties.windowed_height,
                         0);
    properties.fullscreen = false;
    user_data.was_resized = true;
  }
}

auto Window::close() -> void { glfwSetWindowShouldClose(window, GLFW_TRUE); }

auto Window::should_close() const -> bool {
  if (window == nullptr)
    return false;

  return glfwWindowShouldClose(window) != GLFW_FALSE;
}

auto Window::update() -> void { glfwPollEvents(); }

auto Window::get_native() const -> const GLFWwindow * { return window; }
auto Window::get_native() -> GLFWwindow * { return window; }
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

auto Window::set_event_handler(std::function<void(Event &)> &&event_callback)
    -> void {
  user_data.event_callback = std::move(event_callback);
}

auto Window::was_resized() -> bool {
  if (window == nullptr) {
    return false;
  }

  return user_data.was_resized || size_is_zero() ||
         glfwGetWindowAttrib(window, GLFW_ICONIFIED);
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
