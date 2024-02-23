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

struct SerialisationResult {
  std::string reason{};
  bool success{true};

  explicit(false) operator bool() const { return success; }

  SerialisationResult(const std::string &reason_string, bool value)
      : reason(reason_string), success(value) {}

  explicit(false) SerialisationResult(bool value)
      : reason(fmt::format("Success?: {}", value)), success(value) {}
};

#define DESERIALISE_FIELD(field)                                               \
  if (!ECS::read(in, field)) {                                                 \
    return SerialisationResult{                                                \
        fmt::format("Could not deserialise field '{}'", #field),               \
        false,                                                                 \
    };                                                                         \
  }

#define SERIALISE_FIELD(field)                                                 \
  if (!ECS::write(out, field)) {                                               \
    return SerialisationResult{                                                \
        fmt::format("Could not serialise field '{}'", #field),                 \
        false,                                                                 \
    };                                                                         \
  }

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
      -> SerialisationResult {
    SERIALISE_FIELD(component.name);
    return true;
  }

  static auto deserialise(std::istream &in, IdentityComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.name);
    return true;
  }
};

template <> struct ComponentSerialiser<TransformComponent> {
  static auto serialise(const TransformComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.position);
    SERIALISE_FIELD(component.rotation);
    SERIALISE_FIELD(component.scale);
    return true;
  }

  static auto deserialise(std::istream &in, TransformComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.position);
    DESERIALISE_FIELD(component.rotation);
    DESERIALISE_FIELD(component.scale);
    return true;
  }
};

template <> struct ComponentSerialiser<TextureComponent> {
  static auto serialise(const TextureComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.colour);
    return true;
  }

  static auto deserialise(std::istream &in, TextureComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.colour);
    return true;
  }
};

template <> struct ComponentSerialiser<MeshComponent> {
  static auto serialise(const MeshComponent &component, std::ostream &out)
      -> SerialisationResult {
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

  static auto deserialise(std::istream &in, MeshComponent &out)
      -> SerialisationResult {
    bool has_mesh = false;
    DESERIALISE_FIELD(has_mesh);

    if (has_mesh) {
      std::string file_path;
      DESERIALISE_FIELD(file_path);
      out.path = file_path;
    }

    return true;
  }
};

template <> struct ComponentSerialiser<CameraComponent> {
  static auto serialise(const CameraComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.field_of_view);
    SERIALISE_FIELD(component.camera_type);
    SERIALISE_FIELD(component.near);
    SERIALISE_FIELD(component.far);
    return true;
  }

  static auto deserialise(std::istream &in, CameraComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.field_of_view);
    DESERIALISE_FIELD(component.camera_type);
    DESERIALISE_FIELD(component.near);
    DESERIALISE_FIELD(component.far);
    return true;
  }
};

template <> struct ComponentSerialiser<SunComponent> {
  static auto serialise(const SunComponent &component, std::ostream &out)
      -> SerialisationResult {
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

  static auto deserialise(std::istream &in, SunComponent &component)
      -> SerialisationResult {
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
};

template <> struct ComponentSerialiser<ChildComponent> {
  static auto serialise(const ChildComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.children);
    return true;
  }

  static auto deserialise(std::istream &in, ChildComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.children);
    return true;
  }
};

template <> struct ComponentSerialiser<ParentComponent> {
  static auto serialise(const ParentComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.parent);
    return true;
  }

  static auto deserialise(std::istream &in, ParentComponent &component)
      -> SerialisationResult {
    DESERIALISE_FIELD(component.parent);
    return true;
  }
};

template <> struct ComponentSerialiser<PointLightComponent> {
  static auto serialise(const PointLightComponent &component, std::ostream &out)
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

  static auto deserialise(std::istream &in, PointLightComponent &component)
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
};

template <> struct ComponentSerialiser<SpotLightComponent> {
  static auto serialise(const SpotLightComponent &component, std::ostream &out)
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

  static auto deserialise(std::istream &in, SpotLightComponent &component)
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
};

namespace BasicGeometrySerialisation {

auto write(std::ostream &, const BasicGeometry::QuadParameters &)
    -> SerialisationResult;
auto write(std::ostream &, const BasicGeometry::TriangleParameters &)
    -> SerialisationResult;
auto write(std::ostream &, const BasicGeometry::CircleParameters &)
    -> SerialisationResult;
auto write(std::ostream &, const BasicGeometry::SphereParameters &)
    -> SerialisationResult;
auto write(std::ostream &, const BasicGeometry::CubeParameters &)
    -> SerialisationResult;
auto read(std::istream &, BasicGeometry::QuadParameters &)
    -> SerialisationResult;
auto read(std::istream &, BasicGeometry::TriangleParameters &)
    -> SerialisationResult;
auto read(std::istream &, BasicGeometry::CircleParameters &)
    -> SerialisationResult;
auto read(std::istream &, BasicGeometry::SphereParameters &)
    -> SerialisationResult;
auto read(std::istream &, BasicGeometry::CubeParameters &)
    -> SerialisationResult;

} // namespace BasicGeometrySerialisation

template <> struct ComponentSerialiser<GeometryComponent> {
  static auto serialise(const GeometryComponent &component, std::ostream &out)
      -> SerialisationResult {
    SERIALISE_FIELD(component.parameters.index());

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

  static auto deserialise(std::istream &in, GeometryComponent &component)
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
};

} // namespace ECS
