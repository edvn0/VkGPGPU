#pragma once

#include "AABB.hpp"

#include <glm/glm.hpp>

namespace Core {

class Ray {
public:
  glm::vec3 origin;
  glm::vec3 direction;

  // Use a member initializer list for constructor
  constexpr Ray(const glm::vec3 &input_origin, const glm::vec3 &input_direction)
      : origin(input_origin), direction(input_direction) {}

  // Make `zero` a constexpr for potential compile-time initialization
  static constexpr auto zero() { return Ray(glm::vec3(0.0f), glm::vec3(0.0f)); }

  // Simplify AABB intersection check for readability and performance
  bool intersects_aabb(const AABB &aabb, float &t) const {
    glm::vec3 invDir =
        1.0f / direction; // Use inverse direction to avoid division in loop
    const glm::vec3 &lb = aabb.min();
    const glm::vec3 &rt = aabb.max();

    glm::vec3 t0 = (lb - origin) * invDir;
    glm::vec3 t1 = (rt - origin) * invDir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float min_max = glm::max(tmin.x, glm::max(tmin.y, tmin.z));
    float max_min = glm::min(tmax.x, glm::min(tmax.y, tmax.z));

    t = min_max;

    return max_min >= min_max && max_min > 0.0f;
  }

  // Clarify triangle intersection check
  bool intersects_triangle(const glm::vec3 &a, const glm::vec3 &b,
                           const glm::vec3 &c, float &t) const {
    const glm::vec3 e1 = b - a;
    const glm::vec3 e2 = c - a;
    const glm::vec3 h = glm::cross(direction, e2);
    const float dot = glm::dot(e1, h);

    if (dot > -1e-6f && dot < 1e-6f) // Parallel to triangle
      return false;

    const float f = 1.0f / dot;
    const glm::vec3 s = origin - a;
    const float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f)
      return false;

    const glm::vec3 q = glm::cross(s, e1);
    const float v = f * glm::dot(direction, q);

    if (v < 0.0f || u + v > 1.0f)
      return false;

    // At this stage we can compute t to find out where the intersection point
    // is on the line.
    t = f * glm::dot(e2, q);
    return t > 1e-6f; // Ray intersection
  }
};

} // namespace Core
