#pragma once

#include "Types.hpp"

#include <concepts>
#include <glm/glm.hpp>

namespace Core {

enum class AABBAxis : u8 {
  X,
  Y,
  Z,
};

struct AABBRange {
  float min = std::numeric_limits<float>::max();
  float max = std::numeric_limits<float>::lowest();
  constexpr AABBRange(std::floating_point auto min_value,
                      std::floating_point auto max_value)
      : min(min_value), max(max_value) {}

  constexpr AABBRange() = default;
};
class AABB {

public:
  AABB() = default;
  constexpr AABB(const glm::vec2 &for_x, const glm::vec2 &for_y,
                 const glm::vec2 &for_z) noexcept
      : min_max_x(for_x.x, for_x.y), min_max_y(for_y.x, for_y.y),
        min_max_z(for_z.x, for_z.y) {}
  constexpr AABB(const AABBRange &for_x, const AABBRange &for_y,
                 const AABBRange &for_z) noexcept
      : min_max_x(for_x), min_max_y(for_y), min_max_z(for_z) {}
  template <AABBAxis Axis> auto for_axis() const {
    if constexpr (AABBAxis::X == Axis) {
      return min_max_x;
    }
    if constexpr (AABBAxis::Y == Axis) {
      return min_max_y;
    }
    if constexpr (AABBAxis::Z == Axis) {
      return min_max_z;
    }
  }

  void update(const glm::vec3 &vertex_position) {
    min_max_x.min = glm::min(vertex_position.x, min_max_x.min);
    min_max_y.min = glm::min(vertex_position.y, min_max_y.min);
    min_max_z.min = glm::min(vertex_position.z, min_max_z.min);
    // Generate for max
    min_max_x.max = glm::max(vertex_position.x, min_max_x.max);
    min_max_y.max = glm::max(vertex_position.y, min_max_y.max);
    min_max_z.max = glm::max(vertex_position.z, min_max_z.max);
  }

  void update(const glm::vec3 &new_min, const glm::vec3 &new_max) {
    min_max_x.min = glm::min(new_min.x, min_max_x.min);
    min_max_y.min = glm::min(new_min.y, min_max_y.min);
    min_max_z.min = glm::min(new_min.z, min_max_z.min);
    // Generate for max
    min_max_x.max = glm::max(new_max.x, min_max_x.max);
    min_max_y.max = glm::max(new_max.y, min_max_y.max);
    min_max_z.max = glm::max(new_max.z, min_max_z.max);
  }

  [[nodiscard]] auto max_vector() const {
    return glm::vec4{min_max_x.max, min_max_y.max, min_max_z.max, 1.0F};
  }
  [[nodiscard]] auto min_vector() const {
    return glm::vec4{min_max_x.min, min_max_y.min, min_max_z.min, 1.0F};
  }
  [[nodiscard]] auto min() const {
    return glm::vec3{min_max_x.min, min_max_y.min, min_max_z.min};
  }
  [[nodiscard]] auto max() const {
    return glm::vec3{min_max_x.max, min_max_y.max, min_max_z.max};
  }

private:
  AABBRange min_max_x{};
  AABBRange min_max_y{};
  AABBRange min_max_z{};
};

} // namespace Core
