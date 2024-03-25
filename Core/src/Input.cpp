#include "Input.hpp"

#include <GLFW/glfw3.h>

namespace Core {
auto Input::released(KeyCode code) -> bool {
  return glfwGetKey(window, static_cast<int>(code)) == GLFW_RELEASE;
}
auto Input::released(MouseCode code) -> bool {
  return glfwGetMouseButton(window, static_cast<int>(code)) == GLFW_RELEASE;
}
auto Input::pressed(MouseCode code) -> bool {
  return glfwGetMouseButton(window, static_cast<int>(code)) == GLFW_PRESS;
}
auto Input::mouse_position() -> glm::vec2 {
  glm::dvec2 pos;
  glfwGetCursorPos(window, &pos.x, &pos.y);
  return pos;
}
auto Input::pressed(KeyCode code) -> bool {
  return glfwGetKey(window, static_cast<int>(code)) == GLFW_PRESS;
}
} // namespace Core
