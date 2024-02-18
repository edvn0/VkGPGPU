#pragma once

#include "InputCodes.hpp"
#include "Types.hpp"

#include <glm/glm.hpp>

extern "C" {
struct GLFWwindow;
}

namespace Core {

class Input {
public:
  template <KeyCode K> static auto pressed() -> bool { return pressed(K); }
  template <MouseCode M> static auto pressed() -> bool { return pressed(M); }
  template <KeyCode K> static auto released() -> bool { return released(K); }
  template <MouseCode M> static auto released() -> bool { return released(M); }

  static auto released(KeyCode code) -> bool;
  static auto released(MouseCode code) -> bool;
  static auto pressed(MouseCode code) -> bool;
  static auto mouse_position() -> glm::vec2;
  static auto pressed(KeyCode code) -> bool;
  static auto initialise(GLFWwindow *win) { window = win; }

private:
  Input() = delete;
  static inline GLFWwindow *window;
};

} // namespace Core
