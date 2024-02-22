#include "ecs/serialisation/Serialisers.hpp"

namespace ECS {
namespace BasicGeometrySerialisation {
auto write(std::ostream &out, const BasicGeometry::QuadParameters &params)
    -> bool {
  if (!ECS::write(out, params.width))
    return false;
  if (!ECS::write(out, params.height))
    return false;
  return true;
}
auto write(std::ostream &out, const BasicGeometry::TriangleParameters &params)
    -> bool {
  if (!ECS::write(out, params.base))
    return false;
  if (!ECS::write(out, params.height))
    return false;
  return true;
}
auto write(std::ostream &out, const BasicGeometry::CircleParameters &params)
    -> bool {
  if (!ECS::write(out, params.radius))
    return false;
  return true;
}
auto write(std::ostream &out, const BasicGeometry::SphereParameters &params)
    -> bool {
  if (!ECS::write(out, params.radius))
    return false;
  return true;
}
auto write(std::ostream &out, const BasicGeometry::CubeParameters &params)
    -> bool {
  if (!ECS::write(out, params.side_length))
    return false;
  return true;
}
auto read(std::istream &in, BasicGeometry::QuadParameters &params) -> bool {
  if (!ECS::read(in, params.width))
    return false;
  if (!ECS::read(in, params.height))
    return false;
  return true;
}
auto read(std::istream &in, BasicGeometry::TriangleParameters &params) -> bool {
  if (!ECS::read(in, params.base))
    return false;
  if (!ECS::read(in, params.height))
    return false;
  return true;
}
auto read(std::istream &in, BasicGeometry::CircleParameters &params) -> bool {
  if (!ECS::read(in, params.radius))
    return false;
  return true;
}
auto read(std::istream &in, BasicGeometry::SphereParameters &params) -> bool {
  if (!ECS::read(in, params.radius))
    return false;
  return true;
}
auto read(std::istream &in, BasicGeometry::CubeParameters &params) -> bool {
  if (!ECS::read(in, params.side_length))
    return false;
  return true;
}
} // namespace BasicGeometrySerialisation
} // namespace ECS
