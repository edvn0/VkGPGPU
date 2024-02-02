#pragma once
#include "Input.hpp"

#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Core {

class Camera {
public:
  auto update_camera(const float ts) {
    if (Input::pressed(KeyCode::KEY_D)) {
      const glm::quat rotation_y =
          glm::angleAxis(ts * 0.3F, glm::vec3(0, -1, 0));
      camera_orientation = rotation_y * camera_orientation;
    }
    if (Input::pressed(KeyCode::KEY_A)) {
      const glm::quat rotation_y =
          glm::angleAxis(-ts * 0.3F, glm::vec3(0, -1, 0));
      camera_orientation = rotation_y * camera_orientation;
    }
    if (Input::pressed(KeyCode::KEY_W)) {
      const glm::quat rotation_x =
          glm::angleAxis(ts * 0.3F, glm::vec3(-1, 0, 0));
      camera_orientation = rotation_x * camera_orientation;
    }
    if (Input::pressed(KeyCode::KEY_S)) {
      const glm::quat rotation_x =
          glm::angleAxis(-ts * 0.3F, glm::vec3(-1, 0, 0));
      camera_orientation = rotation_x * camera_orientation;
    }

    if (Input::pressed(KeyCode::KEY_Q)) {
      radius -= zoom_speed * ts;
      if (radius < 1.0F)
        radius = 1.0F;
    }

    if (Input::pressed(KeyCode::KEY_E)) {
      radius += zoom_speed * ts;
    }

    camera_orientation = glm::normalize(camera_orientation);
    const glm::vec3 direction =
        glm::rotate(camera_orientation, glm::vec3(0, 0, -1));
    camera_position = direction * radius;
  }

  [[nodiscard]] auto get_view_matrix() const -> glm::mat4 {
    return glm::lookAt(camera_position, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});
  }

  [[nodiscard]] auto get_projection_matrix() const -> glm::mat4 {
    return glm::perspective(glm::radians(fov), aspect_ratio, far, near);
  }

  [[nodiscard]] auto get_camera_position() -> glm::vec3 & {
    return camera_position;
  }

  auto set_aspect_ratio(const float aspect_ratio) -> void {
    this->aspect_ratio = aspect_ratio;
  }

private:
  glm::vec3 camera_position{-7, 8, 2};
  static constexpr float zoom_speed = 1.0F;
  float radius = 17.0F;
  glm::quat camera_orientation{1.0, 0.0, 0.0, 0.0};

  float near = 0.1F;
  float aspect_ratio = 1280.0F / 720.0F;
  float far = 100.0F;
  float fov = 45.0F;
};

} // namespace Core
