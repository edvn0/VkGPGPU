#pragma once
#include "Concepts.hpp"
#include "Logger.hpp"
#include "Math.hpp"
#include "Types.hpp"

#include <bit>
#include <entt/entt.hpp>
#include <istream>
#include <magic_enum.hpp>
#include <ostream>

#include "ecs/components/Component.hpp"

namespace ECS {

namespace Detail {
template <typename T> bool write(std::ostream &out, const T &value) {
  static_assert(std::is_same_v<T, void>, "No write function found for type.");
  return false;
}

// Base deserialize function to be specialized or found via ADL
template <typename T> bool read(std::istream &in, T &value) {
  static_assert(std::is_same_v<T, void>, "No write function found for type.");
  return false;
}
} // namespace Detail

template <typename T>
concept Serializable = requires(const T &a, std::ostream &os) {
  { Detail::write(os, a) } -> std::same_as<bool>;
};

template <typename T>
concept Deserializable = requires(T &a, std::istream &is) {
  { Detail::read(is, a) } -> std::same_as<bool>;
};

template <typename T>
concept TwoWaySerializable = Serializable<T> && Deserializable<T>;

template <typename T>
  requires(std::floating_point<std::decay_t<T>> ||
           std::integral<std::decay_t<T>>)
bool write(std::ostream &out, const T &value) {
  if (!out.write(std::bit_cast<const char *>(&value), sizeof(value))) {
    error("Failed to write value to stream.");
    return false;
  }
  return true;
}

template <typename T>
  requires(std::floating_point<std::decay_t<T>> ||
           std::integral<std::decay_t<T>>)
bool read(std::istream &in, T &value) {
  if (!in.read(std::bit_cast<char *>(&value), sizeof(value))) {
    error("Failed to read value from stream.");
    return false;
  }
  return true;
}

// BEGIN GLM TYPES
template <glm::length_t L, typename T, glm::qualifier Q>
auto write(std::ostream &out, const glm::vec<L, T, Q> &vec) -> bool {
  if (!out.write(std::bit_cast<const char *>(Core::Math::value_ptr(vec)),
                 sizeof(vec)))
    return false;
  return true;
}

template <glm::length_t L, typename T, glm::qualifier Q>
auto read(std::istream &in, glm::vec<L, T, Q> &vec) -> bool {
  if (!in.read(std::bit_cast<char *>(Core::Math::value_ptr(vec)), sizeof(vec)))
    return false;
  return true;
}

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
auto write(std::ostream &out, const glm::mat<C, R, T, Q> &mat) -> bool {
  if (!out.write(std::bit_cast<const char *>(Core::Math::value_ptr(mat)),
                 sizeof(mat))) {

    return false;
  }
  return true;
}
template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
auto read(std::istream &in, glm::mat<C, R, T, Q> &mat) -> bool {
  if (!in.read(std::bit_cast<char *>(Core::Math::value_ptr(mat)), sizeof(mat)))
    return false;
  return true;
}

template <typename T, glm::qualifier Q>
auto write(std::ostream &out, const glm::qua<T, Q> &quat) -> bool {
  if (!out.write(std::bit_cast<const char *>(Core::Math::value_ptr(quat)),
                 sizeof(quat))) {
    return false;
  }
  return true;
}

template <typename T, glm::qualifier Q>
static bool read(std::istream &in, glm::qua<T, Q> &quat) {
  if (!in.read(std::bit_cast<char *>(Core::Math::value_ptr(quat)),
               sizeof(quat)))
    return false;
  return true;
}
// END GLM TYPES

// String
inline auto write(std::ostream &out, const std::string &str) -> bool {
  const auto size = str.size();
  if (!write(out, size))
    return false;
  if (!out.write(str.data(), size)) {
    error("Failed to write string to stream.");
    return false;
  }
  return true;
}

inline auto read(std::istream &in, std::string &str) -> bool {
  std::size_t size;
  if (!read(in, size))
    return false;
  str.resize(size);
  if (!in.read(str.data(), size)) {
    error("Failed to read string from stream.");
    return false;
  }
  return true;
}

// Begin std (vector, unordered_map, set, map, array)
template <TwoWaySerializable T>
auto write(std::ostream &out, const std::vector<T> &vec) -> bool {
  const auto size = vec.size();
  if (!write(out, size))
    return false;
  for (const T &element : vec) {
    if (!write(out, element))
      return false;
  }
  return true;
}

template <TwoWaySerializable T>
auto read(std::istream &in, std::vector<T> &vec) -> bool {
  std::size_t size;
  if (!read(in, size))
    return false;
  vec.resize(size);
  for (auto &element : vec) {
    if (!read(in, element))
      return false;
  }
  return true;
}

template <TwoWaySerializable T, TwoWaySerializable U>
auto write(std::ostream &out, const std::unordered_map<T, U> &map) -> bool {
  const auto size = map.size();
  if (!write(out, size))
    return false;
  for (const auto &[key, value] : map) {
    if (!write(out, key))
      return false;
    if (!write(out, value))
      return false;
  }
  return true;
}

template <TwoWaySerializable T, TwoWaySerializable U>
auto read(std::istream &in, std::unordered_map<T, U> &map) -> bool {
  std::size_t size;
  if (!read(in, size))
    return false;
  for (std::size_t i = 0; i < size; ++i) {
    T key;
    U value;
    if (!read(in, key))
      return false;
    if (!read(in, value))
      return false;
    map[key] = value;
  }
  return true;
}
// End std

// Enums?

template <Core::IsEnum E>
bool is_valid_enum_value(std::underlying_type_t<E> value) {
  return magic_enum::enum_contains<E>(value);
}

template <Core::IsEnum E>
auto write(std::ostream &out, const E &enumerated_value) -> bool {
  static_assert(std::is_enum_v<E>, "E must be an enum type.");
  auto underlying = static_cast<std::underlying_type_t<E>>(enumerated_value);
  return write(out, underlying); // Reuse write for underlying type
}

template <Core::IsEnum E> bool read(std::istream &in, E &enumerated_value) {
  static_assert(std::is_enum_v<E>, "E must be an enum type.");
  std::underlying_type_t<E> underlying_value;
  if (!read(in, underlying_value)) { // Reuse read for underlying type
    return false;
  }

  if (!is_valid_enum_value<E>(underlying_value)) {
    error("Invalid enum value read from stream.");
    return false;
  }

  auto enum_value_opt = magic_enum::enum_cast<E>(underlying_value);
  if (!enum_value_opt.has_value()) {
    error("Invalid enum value read from stream.");
    return false;
  }

  enumerated_value = enum_value_opt.value();
  return true;
}

template <class T> struct ComponentSerialiser;

template <> struct ComponentSerialiser<IdentityComponent> {
  static auto serialise(const IdentityComponent &component, std::ostream &out)
      -> bool {
    if (!write(out, component.name))
      return false;
    return true;
  }

  static auto deserialise(std::istream &in, IdentityComponent &component)
      -> bool {
    if (!read(in, component.name)) {
      return false;
    }
    return true;
  }
};

template <> struct ComponentSerialiser<TransformComponent> {
  static auto serialise(const TransformComponent &component, std::ostream &out)
      -> bool {
    if (!write(out, component.position))
      return false;
    if (!write(out, component.rotation))
      return false;
    if (!write(out, component.scale))
      return false;
    return true;
  }

  static auto deserialise(std::istream &in, TransformComponent &out) -> bool {
    if (!read(in, out.position))
      return false;
    if (!read(in, out.rotation))
      return false;
    if (!read(in, out.scale))
      return false;
    return true;
  }
};

template <> struct ComponentSerialiser<TextureComponent> {
  static auto serialise(const TextureComponent &component,
                        std::ostream &out_stream) -> bool {
    if (!write(out_stream, component.colour))
      return false;

    return true;
  }

  static auto deserialise(std::istream &in, TextureComponent &out) -> bool {
    if (!read(in, out.colour))
      return false;
    return true;
  }
};

template <> struct ComponentSerialiser<MeshComponent> {
  static auto serialise(const MeshComponent &component,
                        std::ostream &out_stream) -> bool {
    const auto has_valid_mesh_path =
        component.mesh != nullptr || !component.path.empty();
    if (!write(out_stream, has_valid_mesh_path))
      return false;

    if (has_valid_mesh_path) {
      if (component.mesh != nullptr) {
        if (const auto &mesh = *component.mesh;
            !write(out_stream, mesh.get_file_path().string()))
          return false;
      } else {
        if (!write(out_stream, component.path.string()))
          return false;
      }
    }

    return true;
  }

  static auto deserialise(std::istream &in, MeshComponent &out) -> bool {
    bool has_mesh = false;
    if (!read(in, has_mesh))
      return false;

    if (has_mesh) {
      std::string file_path;
      if (!read(in, file_path))
        return false;
      out.path = file_path;
    }

    return true;
  }
};

template <> struct ComponentSerialiser<CameraComponent> {
  static auto serialise(const CameraComponent &component,
                        std::ostream &out_stream) -> bool {
    if (!write(out_stream, component.field_of_view))
      return false;
    if (!write(out_stream, component.camera_type))
      return false;
    if (!write(out_stream, component.near))
      return false;
    if (!write(out_stream, component.far))
      return false;
    return true;
  }

  static auto deserialise(std::istream &in, CameraComponent &out) -> bool {
    if (!read(in, out.field_of_view))
      return false;
    if (!read(in, out.camera_type))
      return false;
    if (!read(in, out.near))
      return false;
    if (!read(in, out.far))
      return false;
    return true;
  }
};

template <> struct ComponentSerialiser<SunComponent> {
  static auto serialise(const SunComponent &component, std::ostream &out_stream)
      -> bool {
    if (!write(out_stream, component.direction))
      return false;
    if (!write(out_stream, component.colour))
      return false;
    if (!write(out_stream, component.specular_colour))
      return false;
    if (!write(out_stream, component.depth_params.bias)) {
      return false;
    }
    if (!write(out_stream, component.depth_params.default_value)) {
      return false;
    }
    if (!write(out_stream, component.depth_params.lrbt)) {
      return false;
    }
    if (!write(out_stream, component.depth_params.nf)) {
      return false;
    }
    if (!write(out_stream, component.depth_params.center)) {
      return false;
    }
    return true;
  }

  static auto deserialise(std::istream &in, SunComponent &out) -> bool {
    if (!read(in, out.direction))
      return false;
    if (!read(in, out.colour))
      return false;
    if (!read(in, out.specular_colour))
      return false;
    if (!read(in, out.depth_params.bias)) {
      return false;
    }
    if (!read(in, out.depth_params.default_value)) {
      return false;
    }
    if (!read(in, out.depth_params.lrbt)) {
      return false;
    }
    if (!read(in, out.depth_params.nf)) {
      return false;
    }
    if (!read(in, out.depth_params.center)) {
      return false;
    }
    return true;
  }
};

template <> struct ComponentSerialiser<ChildComponent> {
  static auto serialise(const ChildComponent &component, std::ostream &out)
      -> bool {
    // Serialize the number of children
    const auto size = component.children.size();
    if (!write(out, size))
      return false;

    // Serialize each child entity handle
    for (const auto &child : component.children) {
      if (!write(out, child))
        return false;
    }

    return true;
  }

  static auto deserialise(std::istream &in, ChildComponent &component) -> bool {
    std::size_t size;
    // Deserialize the number of children
    if (!read(in, size))
      return false;

    component.children.resize(size);
    // Deserialize each child entity handle
    for (auto &child : component.children) {
      if (!read(in, child))
        return false;
    }

    return true;
  }
};

template <> struct ComponentSerialiser<ParentComponent> {
  static auto serialise(const ParentComponent &component, std::ostream &out)
      -> bool {
    // Serialize the parent entity handle
    if (!write(out, component.parent))
      return false;
    return true;
  }

  static auto deserialise(std::istream &in, ParentComponent &component)
      -> bool {
    // Deserialize the parent entity handle
    if (!read(in, component.parent))
      return false;
    return true;
  }
};

template <> struct ComponentSerialiser<PointLightComponent> {
  static auto serialise(const PointLightComponent &component, std::ostream &out)
      -> bool {
    if (!write(out, component.radiance)) {
      return false;
    }
    if (!write(out, component.intensity)) {
      return false;
    }
    if (!write(out, component.light_size)) {
      return false;
    }
    if (!write(out, component.min_radius)) {
      return false;
    }
    if (!write(out, component.radius)) {
      return false;
    }
    if (!write(out, component.casts_shadows)) {
      return false;
    }
    if (!write(out, component.soft_shadows)) {
      return false;
    }
    if (!write(out, component.falloff)) {
      return false;
    }
    return true;
  }

  static auto deserialise(std::istream &in, PointLightComponent &component)
      -> bool {
    // Deserialize the parent entity handle
    if (!read(in, component.radiance)) {
      return false;
    }
    if (!read(in, component.intensity)) {
      return false;
    }
    if (!read(in, component.light_size)) {
      return false;
    }
    if (!read(in, component.min_radius)) {
      return false;
    }
    if (!read(in, component.radius)) {
      return false;
    }
    if (!read(in, component.casts_shadows)) {
      return false;
    }
    if (!read(in, component.soft_shadows)) {
      return false;
    }
    if (!read(in, component.falloff)) {
      return false;
    }
    return true;
  }
};

template <> struct ComponentSerialiser<SpotLightComponent> {
  static auto serialise(const SpotLightComponent &component, std::ostream &out)
      -> bool {
    if (!write(out, component.radiance)) {
      return false;
    }
    if (!write(out, component.intensity)) {
      return false;
    }
    if (!write(out, component.range)) {
      return false;
    }
    if (!write(out, component.angle)) {
      return false;
    }
    if (!write(out, component.angle_attenuation)) {
      return false;
    }
    if (!write(out, component.casts_shadows)) {
      return false;
    }
    if (!write(out, component.soft_shadows)) {
      return false;
    }
    if (!write(out, component.falloff)) {
      return false;
    }
    return true;
  }

  static auto deserialise(std::istream &in, SpotLightComponent &component)
      -> bool {
    // Deserialize the parent entity handle
    if (!read(in, component.radiance)) {
      return false;
    }
    if (!read(in, component.intensity)) {
      return false;
    }
    if (!read(in, component.range)) {
      return false;
    }
    if (!read(in, component.angle)) {
      return false;
    }
    if (!read(in, component.angle_attenuation)) {
      return false;
    }
    if (!read(in, component.casts_shadows)) {
      return false;
    }
    if (!read(in, component.soft_shadows)) {
      return false;
    }
    if (!read(in, component.falloff)) {
      return false;
    }
    return true;
  }
};

namespace BasicGeometrySerialisation {

auto write(std::ostream &, const BasicGeometry::QuadParameters &) -> bool;
auto write(std::ostream &, const BasicGeometry::TriangleParameters &) -> bool;
auto write(std::ostream &, const BasicGeometry::CircleParameters &) -> bool;
auto write(std::ostream &, const BasicGeometry::SphereParameters &) -> bool;
auto write(std::ostream &, const BasicGeometry::CubeParameters &) -> bool;
auto read(std::istream &, BasicGeometry::QuadParameters &) -> bool;
auto read(std::istream &, BasicGeometry::TriangleParameters &) -> bool;
auto read(std::istream &, BasicGeometry::CircleParameters &) -> bool;
auto read(std::istream &, BasicGeometry::SphereParameters &) -> bool;
auto read(std::istream &, BasicGeometry::CubeParameters &) -> bool;

} // namespace BasicGeometrySerialisation

template <> struct ComponentSerialiser<GeometryComponent> {
  static bool serialise(const GeometryComponent &component, std::ostream &out) {
    auto type = component.parameters.index();
    if (!write(out, type))
      return false;

    // Serialize the actual data based on the type
    return std::visit(
        Core::overloaded{
            [&](const BasicGeometry::QuadParameters &quad) {
              return BasicGeometrySerialisation::write(out, quad);
            },
            [&](const BasicGeometry::TriangleParameters &triangle) {
              return BasicGeometrySerialisation::write(out, triangle);
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

  static bool deserialise(std::istream &in, GeometryComponent &component) {
    // Deserialize the type of the geometry first
    std::size_t type;
    if (!read(in, type))
      return false;

    switch (type) {
    case 0: {
      BasicGeometry::QuadParameters quad;
      if (BasicGeometrySerialisation::read(in, quad)) {
        component.parameters = quad;
        return true;
      }
      break;
    }
    case 1: {
      BasicGeometry::TriangleParameters triangle;
      if (BasicGeometrySerialisation::read(in, triangle)) {
        component.parameters = triangle;
        return true;
      }
      break;
    }
    case 2: {
      BasicGeometry::CircleParameters circle;
      if (BasicGeometrySerialisation::read(in, circle)) {
        component.parameters = circle;
        return true;
      }
      break;
    }
    case 3: {
      BasicGeometry::SphereParameters sphere;
      if (BasicGeometrySerialisation::read(in, sphere)) {
        component.parameters = sphere;
        return true;
      }
      break;
    }
    case 4: {
      BasicGeometry::CubeParameters cube;
      if (BasicGeometrySerialisation::read(in, cube)) {
        component.parameters = cube;
        return true;
      }
      break;
    }
    default:
      assert(false && "Unknown geometry type");
      return false;
    }

    return false;
  }
};

} // namespace ECS
