#include "pch/vkgpgpu_pch.hpp"

#include "ecs/components/Component.hpp"

#include <glm/gtx/quaternion.hpp>

namespace ECS {

auto TransformComponent::compute() const -> glm::mat4 {
  return glm::translate(glm::mat4(1.0F), position) * glm::toMat4(rotation) *
         glm::scale(glm::mat4(1.0F), scale);
}

auto TransformComponent::update(const glm::vec3 &pos, const glm::quat &rot,
                                const glm::vec3 &scl) -> void {
  position = pos;
  rotation = rot;
  scale = scl;
}

auto BasicGeometry::get_aabb_for_geometry(
    const ECS::BasicGeometry::GeometryVariant &geom, const glm::mat4 &transform)
    -> Core::AABB {
  Core::AABB aabb;

  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        std::vector<glm::vec3>
            points; // Used for geometries defined by vertices

        if constexpr (std::is_same_v<T, ECS::BasicGeometry::QuadParameters>) {
          // Assuming Quad is centered at origin; adjust if not
          float half_width = arg.width / 2.0f;
          float half_height = arg.height / 2.0f;
          points = {
              {
                  -half_width,
                  -half_height,
                  0.0f,
              },
              {
                  half_width,
                  half_height,
                  0.0f,
              },
          };
        } else if constexpr (std::is_same_v<
                                 T, ECS::BasicGeometry::TriangleParameters>) {
          // Assuming Triangle is equilateral and centered at origin; adjust if
          // not
          float half_base = arg.base / 2.0f;
          points = {
              {
                  -half_base,
                  0.0f,
                  0.0f,
              },
              {
                  half_base,
                  0.0f,
                  0.0f,
              },
              {
                  0.0f,
                  arg.height,
                  0.0f,
              },
          };
        } else if constexpr (std::is_same_v<
                                 T, ECS::BasicGeometry::CircleParameters> ||
                             std::is_same_v<
                                 T, ECS::BasicGeometry::SphereParameters>) {
          // For Circle/Sphere, adjust AABB based on radius and scale; assuming
          // centered at origin
          glm::vec3 scale_factor =
              glm::vec3(transform[0][0], transform[1][1], transform[2][2]) *
              arg.radius;
          glm::vec3 center =
              glm::vec3(transform[3]); // Translation component of the matrix
          glm::vec3 min = center - scale_factor;
          glm::vec3 max = center + scale_factor;
          aabb.update(min, max);
          return; // Early return as we've already calculated AABB
        } else if constexpr (std::is_same_v<
                                 T, ECS::BasicGeometry::CubeParameters>) {
          float half_size = arg.side_length / 2.0f;
          points = {
              {
                  -half_size,
                  -half_size,
                  -half_size,
              },
              {
                  half_size,
                  half_size,
                  half_size,
              },
          };
        }

        // Transform points and update AABB for non-spherical objects
        for (auto &point : points) {
          glm::vec4 world_point = transform * glm::vec4(point, 1.0f);
          aabb.update(glm::vec3(world_point));
        }
      },
      geom);

  return aabb;
}

} // namespace ECS
