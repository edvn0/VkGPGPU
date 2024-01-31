#pragma once

#include "Types.hpp"

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

} // namespace Core
