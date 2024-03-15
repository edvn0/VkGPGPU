#include "ecs/serialisation/GeneralBinarySerialisers.hpp"
#include "ecs/serialisation/Serialisers.hpp"

namespace ECS {

auto ComponentSerialiser<SerialisationType::Binary, IdentityComponent>::
    serialise(const IdentityComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.name);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, IdentityComponent>::
    deserialise(std::istream &in, IdentityComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.name);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, TransformComponent>::
    serialise(const TransformComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.position);
  SERIALISE_FIELD(component.rotation);
  SERIALISE_FIELD(component.scale);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, TransformComponent>::
    deserialise(std::istream &in, TransformComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.position);
  DESERIALISE_FIELD(component.rotation);
  DESERIALISE_FIELD(component.scale);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, TextureComponent>::
    serialise(const TextureComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.colour);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, TextureComponent>::
    deserialise(std::istream &in, TextureComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.colour);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, MeshComponent>::serialise(
    const MeshComponent &component, std::ostream &out) -> SerialisationResult {
  const auto has_valid_mesh_path =
      component.mesh != nullptr || !component.path.empty();
  SERIALISE_FIELD(has_valid_mesh_path);

  if (has_valid_mesh_path) {
    if (component.mesh != nullptr) {
      const auto &mesh = *component.mesh;
      SERIALISE_FIELD(mesh.get_file_path().string())
    } else {
      SERIALISE_FIELD(component.path.string())
    }
  }

  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, MeshComponent>::deserialise(
    std::istream &in, MeshComponent &out) -> SerialisationResult {
  bool has_mesh = false;
  DESERIALISE_FIELD(has_mesh);

  if (has_mesh) {
    std::string file_path;
    DESERIALISE_FIELD(file_path);
    out.path = file_path;
  }

  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, CameraComponent>::serialise(
    const CameraComponent &component, std::ostream &out)
    -> SerialisationResult {
  SERIALISE_FIELD(component.field_of_view);
  SERIALISE_FIELD(component.camera_type);
  SERIALISE_FIELD(component.near);
  SERIALISE_FIELD(component.far);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, CameraComponent>::
    deserialise(std::istream &in, CameraComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.field_of_view);
  DESERIALISE_FIELD(component.camera_type);
  DESERIALISE_FIELD(component.near);
  DESERIALISE_FIELD(component.far);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, SunComponent>::serialise(
    const SunComponent &component, std::ostream &out) -> SerialisationResult {
  SERIALISE_FIELD(component.direction);
  SERIALISE_FIELD(component.colour);
  SERIALISE_FIELD(component.specular_colour);
  SERIALISE_FIELD(component.depth_params.bias);
  SERIALISE_FIELD(component.depth_params.default_value);
  SERIALISE_FIELD(component.depth_params.lrbt);
  SERIALISE_FIELD(component.depth_params.nf);
  SERIALISE_FIELD(component.depth_params.center);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, SunComponent>::deserialise(
    std::istream &in, SunComponent &component) -> SerialisationResult {
  DESERIALISE_FIELD(component.direction);
  DESERIALISE_FIELD(component.colour);
  DESERIALISE_FIELD(component.specular_colour);
  DESERIALISE_FIELD(component.depth_params.bias);
  DESERIALISE_FIELD(component.depth_params.default_value);
  DESERIALISE_FIELD(component.depth_params.lrbt);
  DESERIALISE_FIELD(component.depth_params.nf);
  DESERIALISE_FIELD(component.depth_params.center);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, ChildComponent>::serialise(
    const ChildComponent &component, std::ostream &out) -> SerialisationResult {
  SERIALISE_FIELD(component.children);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary,
                         ChildComponent>::deserialise(std::istream &in,
                                                      ChildComponent &component)
    -> SerialisationResult {
  DESERIALISE_FIELD(component.children);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, ParentComponent>::serialise(
    const ParentComponent &component, std::ostream &out)
    -> SerialisationResult {
  SERIALISE_FIELD(component.parent);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, ParentComponent>::
    deserialise(std::istream &in, ParentComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.parent);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, PointLightComponent>::
    serialise(const PointLightComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.radiance);
  SERIALISE_FIELD(component.intensity);
  SERIALISE_FIELD(component.light_size);
  SERIALISE_FIELD(component.min_radius);
  SERIALISE_FIELD(component.radius);
  SERIALISE_FIELD(component.casts_shadows);
  SERIALISE_FIELD(component.soft_shadows);
  SERIALISE_FIELD(component.falloff);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, PointLightComponent>::
    deserialise(std::istream &in, PointLightComponent &component)
        -> SerialisationResult {
  // Deserialize the parent entity handle
  DESERIALISE_FIELD(component.radiance);
  DESERIALISE_FIELD(component.intensity);
  DESERIALISE_FIELD(component.light_size);
  DESERIALISE_FIELD(component.min_radius);
  DESERIALISE_FIELD(component.radius);
  DESERIALISE_FIELD(component.casts_shadows);
  DESERIALISE_FIELD(component.soft_shadows);
  DESERIALISE_FIELD(component.falloff);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, SpotLightComponent>::
    serialise(const SpotLightComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.radiance);
  SERIALISE_FIELD(component.intensity);
  SERIALISE_FIELD(component.range);
  SERIALISE_FIELD(component.angle);
  SERIALISE_FIELD(component.angle_attenuation);
  SERIALISE_FIELD(component.casts_shadows);
  SERIALISE_FIELD(component.soft_shadows);
  SERIALISE_FIELD(component.falloff);
  return true;
}

auto ComponentSerialiser<SerialisationType::Binary, SpotLightComponent>::
    deserialise(std::istream &in, SpotLightComponent &component)
        -> SerialisationResult {
  DESERIALISE_FIELD(component.radiance);
  DESERIALISE_FIELD(component.intensity);
  DESERIALISE_FIELD(component.range);
  DESERIALISE_FIELD(component.angle);
  DESERIALISE_FIELD(component.angle_attenuation);
  DESERIALISE_FIELD(component.casts_shadows);
  DESERIALISE_FIELD(component.soft_shadows);
  DESERIALISE_FIELD(component.falloff);
  return true;
}

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

auto ComponentSerialiser<SerialisationType::Binary, GeometryComponent>::
    serialise(const GeometryComponent &component, std::ostream &out)
        -> SerialisationResult {
  SERIALISE_FIELD(component.parameters.index());

  // Serialize the actual data based on the type
  return std::visit(
      Core::overloaded{[&](const BasicGeometry::QuadParameters &quad) {
                         return BasicGeometrySerialisation::write(out, quad);
                       },
                       [&](const BasicGeometry::TriangleParameters &triangle) {
                         return BasicGeometrySerialisation::write(out,
                                                                  triangle);
                       },
                       [&](const BasicGeometry::CircleParameters &circle) {
                         return BasicGeometrySerialisation::write(out, circle);
                       },
                       [&](const BasicGeometry::SphereParameters &sphere) {
                         return BasicGeometrySerialisation::write(out, sphere);
                       },
                       [&](const BasicGeometry::CubeParameters &cube) {
                         return BasicGeometrySerialisation::write(out, cube);
                       }},
      component.parameters);
}

auto ComponentSerialiser<SerialisationType::Binary, GeometryComponent>::
    deserialise(std::istream &in, GeometryComponent &component)
        -> SerialisationResult {
  // Deserialize the type of the geometry first
  std::size_t variant_type{};
  DESERIALISE_FIELD(variant_type);

  switch (variant_type) {
  case 0: {
    BasicGeometry::QuadParameters quad;
    if (const auto result = BasicGeometrySerialisation::read(in, quad)) {
      component.parameters = quad;
      return result;
    }
    break;
  }
  case 1: {
    BasicGeometry::TriangleParameters triangle;
    if (const auto result = BasicGeometrySerialisation::read(in, triangle)) {
      component.parameters = triangle;
      return result;
    }
    break;
  }
  case 2: {
    BasicGeometry::CircleParameters circle;
    if (const auto result = BasicGeometrySerialisation::read(in, circle)) {
      component.parameters = circle;
      return result;
    }
    break;
  }
  case 3: {
    BasicGeometry::SphereParameters sphere;
    if (const auto result = BasicGeometrySerialisation::read(in, sphere)) {
      component.parameters = sphere;
      return result;
    }
    break;
  }
  case 4: {
    BasicGeometry::CubeParameters cube;
    if (const auto result = BasicGeometrySerialisation::read(in, cube)) {
      component.parameters = cube;
      return result;
    }
    break;
  }
  default:
    assert(false && "Unknown geometry type");
    return false;
  }

  return false;
}
}; // namespace ECS
