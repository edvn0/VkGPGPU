#include "ecs/serialisation/Serialisers.hpp"

namespace ECS {

namespace BasicGeometrySerialisation {
auto write(std::ostream &out, const BasicGeometry::QuadParameters &params)
    -> SerialisationResult {
  SERIALISE_FIELD(params.width);
  SERIALISE_FIELD(params.height);
  return true;
}
auto write(std::ostream &out, const BasicGeometry::TriangleParameters &params)
    -> SerialisationResult {
  SERIALISE_FIELD(params.base);
  SERIALISE_FIELD(params.height);
  return true;
}
auto write(std::ostream &out, const BasicGeometry::CircleParameters &params)
    -> SerialisationResult {
  SERIALISE_FIELD(params.radius);
  return true;
}
auto write(std::ostream &out, const BasicGeometry::SphereParameters &params)
    -> SerialisationResult {
  SERIALISE_FIELD(params.radius);
  return true;
}
auto write(std::ostream &out, const BasicGeometry::CubeParameters &params)
    -> SerialisationResult {
  SERIALISE_FIELD(params.side_length);
  return true;
}
auto read(std::istream &in, BasicGeometry::QuadParameters &params)
    -> SerialisationResult {
  DESERIALISE_FIELD(params.width);
  DESERIALISE_FIELD(params.height);
  return true;
}
auto read(std::istream &in, BasicGeometry::TriangleParameters &params)
    -> SerialisationResult {
  DESERIALISE_FIELD(params.base);
  DESERIALISE_FIELD(params.height);
  return true;
}
auto read(std::istream &in, BasicGeometry::CircleParameters &params)
    -> SerialisationResult {
  DESERIALISE_FIELD(params.radius);
  return true;
}
auto read(std::istream &in, BasicGeometry::SphereParameters &params)
    -> SerialisationResult {
  DESERIALISE_FIELD(params.radius);
  return true;
}
auto read(std::istream &in, BasicGeometry::CubeParameters &params)
    -> SerialisationResult {
  DESERIALISE_FIELD(params.side_length);
  return true;
}
} // namespace BasicGeometrySerialisation
} // namespace ECS
