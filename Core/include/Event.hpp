#pragma once

#include "Types.hpp"

#include <fmt/core.h>
#include <functional>
#include <string>
#include <string_view>

namespace Core {

enum class EventType {
  None = 0,
  WindowClose,
  WindowMinimize,
  WindowResize,
  WindowFocus,
  WindowLostFocus,
  WindowMoved,
  WindowTitleBarHitTest,
  AppTick,
  AppUpdate,
  AppRender,
  KeyPressed,
  KeyReleased,
  KeyTyped,
  MouseButtonPressed,
  MouseButtonReleased,
  MouseMoved,
  MouseScrolled,
  ScenePreStart,
  ScenePostStart,
  ScenePreStop,
  ScenePostStop,
  SelectionChanged
};

#define BIT(x) (1 << x)

enum EventCategory {
  None = 0,
  EventCategoryApplication = BIT(0),
  EventCategoryInput = BIT(1),
  EventCategoryKeyboard = BIT(2),
  EventCategoryMouse = BIT(3),
  EventCategoryMouseButton = BIT(4),
  EventCategoryScene = BIT(5),
  EventCategoryEditor = BIT(6)
};

#undef BIT

class Event {
public:
  bool handled{false};

  virtual ~Event() = default;
  [[nodiscard]] virtual auto get_event_type() const -> EventType = 0;
  [[nodiscard]] virtual auto get_name() const -> std::string_view = 0;
  [[nodiscard]] virtual auto get_category_flags() const -> i32 = 0;
  [[nodiscard]] virtual auto to_string() const -> std::string {
    return std::string{get_name()};
  }

  [[nodiscard]] bool is_in_category(EventCategory category) const {
    return get_category_flags() & category;
  }
};

class KeyTypedEvent;
class MouseScrolledEvent;

class KeyPressedEvent : public Event {
public:
  KeyPressedEvent(i32 keycode, i32 repeat_count)
      : keycode{keycode}, repeat_count{repeat_count} {}
  ~KeyPressedEvent() override = default;

  [[nodiscard]] auto get_category_flags() const -> i32 override {
    return EventCategoryKeyboard | EventCategoryInput;
  }
  [[nodiscard]] auto get_event_type() const -> EventType override {
    return EventType::KeyPressed;
  }
  [[nodiscard]] auto get_name() const -> std::string_view override {
    return "KeyPressedEvent";
  }

  [[nodiscard]] auto to_string() const -> std::string override {
    return fmt::format("KeyPressedEvent: {} ({})", keycode, repeat_count);
  }

  static auto get_static_type() -> EventType { return EventType::KeyPressed; }

  [[nodiscard]] auto get_keycode() const -> i32 { return keycode; }
  [[nodiscard]] auto get_repeat_count() const -> i32 { return repeat_count; }

private:
  i32 keycode;
  i32 repeat_count;
};

class KeyReleasedEvent final : public Event {
public:
  explicit KeyReleasedEvent(i32 keycode) : keycode{keycode} {}
  ~KeyReleasedEvent() override = default;

  [[nodiscard]] auto get_event_type() const -> EventType override {
    return EventType::KeyReleased;
  }
  [[nodiscard]] auto get_name() const -> std::string_view override {
    return "KeyReleasedEvent";
  }
  [[nodiscard]] auto get_category_flags() const -> i32 override {
    return EventCategoryKeyboard | EventCategoryInput;
  }

  [[nodiscard]] auto to_string() const -> std::string override {
    return fmt::format("KeyReleasedEvent: {}", keycode);
  }

  static auto get_static_type() -> EventType { return EventType::KeyReleased; }

  [[nodiscard]] auto get_keycode() const -> i32 { return keycode; }

private:
  i32 keycode;
};

class WindowResizeEvent final : public Event {
public:
  // W, H (i32)
  WindowResizeEvent(i32 w, i32 h) : width{w}, height{h} {}
  ~WindowResizeEvent() override = default;

  [[nodiscard]] auto get_event_type() const -> EventType override {
    return EventType::WindowResize;
  }
  [[nodiscard]] auto get_name() const -> std::string_view override {
    return "WindowResizeEvent";
  }
  [[nodiscard]] auto get_category_flags() const -> i32 override {
    return EventCategoryApplication;
  }

  [[nodiscard]] auto get_width() const -> i32 { return width; }
  [[nodiscard]] auto get_height() const -> i32 { return height; }

  [[nodiscard]] auto to_string() const -> std::string override {
    return fmt::format("WindowResizeEvent: {}x{}", width, height);
  }

  static auto get_static_type() -> EventType { return EventType::WindowResize; }

private:
  i32 width{0};
  i32 height{0};
};

class MouseScrolledEvent final : public Event {
public:
  MouseScrolledEvent(floating x_offset, floating y_offset)
      : x_offset{x_offset}, y_offset{y_offset} {}
  ~MouseScrolledEvent() override = default;

  [[nodiscard]] auto get_event_type() const -> EventType override {
    return EventType::MouseScrolled;
  }
  [[nodiscard]] auto get_name() const -> std::string_view override {
    return "MouseScrolledEvent";
  }
  [[nodiscard]] auto get_category_flags() const -> i32 override {
    return EventCategoryMouse | EventCategoryInput;
  }

  [[nodiscard]] auto get_x_offset() const -> floating { return x_offset; }
  [[nodiscard]] auto get_y_offset() const -> floating { return y_offset; }

  [[nodiscard]] auto to_string() const -> std::string override {
    return fmt::format("MouseScrolledEvent: ({}, {})", x_offset, y_offset);
  }

  static auto get_static_type() -> EventType {
    return EventType::MouseScrolled;
  }

private:
  floating x_offset{0.0F};
  floating y_offset{0.0F};
};

template <class T>
concept IsEvent = requires {
  { T::get_static_type() } -> std::same_as<EventType>;
};

class EventDispatcher {
  template <typename T> using EventFn = std::function<bool(T &)>;

public:
  explicit EventDispatcher(Event &event) : current_event(event) {}

  template <IsEvent T> bool dispatch(EventFn<T> func) {
    if (current_event.get_event_type() == T::get_static_type() &&
        !current_event.handled) {
      current_event.handled = func(*static_cast<T *>(&current_event));
      return true;
    }
    return false;
  }

private:
  Event &current_event;
};

} // namespace Core
