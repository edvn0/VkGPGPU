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

enum class SerialisationType : Core::u8 {
  Binary,
  JSON,
  YML,
};

struct SerialisationResult {
  std::string reason{};
  bool success{true};

  explicit(false) operator bool() const { return success; }

  SerialisationResult(const std::string &reason_string, bool value)
      : reason(reason_string), success(value) {}

  explicit(false) SerialisationResult(bool value)
      : reason(fmt::format("Success?: {}", value)), success(value) {}
};

template <SerialisationType ST, class T> struct ComponentSerialiser;

#define GPGPU_BINARY_SERIALISATION_SUPPORT
#ifdef GPGPU_BINARY_SERIALISATION_SUPPORT
template <>
struct ComponentSerialiser<SerialisationType::Binary, IdentityComponent> {
  static auto serialise(const IdentityComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, IdentityComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, TransformComponent> {
  static auto serialise(const TransformComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, TransformComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, TextureComponent> {
  static auto serialise(const TextureComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, TextureComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, MeshComponent> {
  static auto serialise(const MeshComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, MeshComponent &out)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, CameraComponent> {
  static auto serialise(const CameraComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, CameraComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, SunComponent> {
  static auto serialise(const SunComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, SunComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, ChildComponent> {
  static auto serialise(const ChildComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, ChildComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, ParentComponent> {
  static auto serialise(const ParentComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, ParentComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, PointLightComponent> {
  static auto serialise(const PointLightComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, PointLightComponent &component)
      -> SerialisationResult;
};

template <>
struct ComponentSerialiser<SerialisationType::Binary, SpotLightComponent> {
  static auto serialise(const SpotLightComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, SpotLightComponent &component)
      -> SerialisationResult;
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

template <>
struct ComponentSerialiser<SerialisationType::Binary, GeometryComponent> {
  static auto serialise(const GeometryComponent &component, std::ostream &out)
      -> SerialisationResult;

  static auto deserialise(std::istream &in, GeometryComponent &component)
      -> SerialisationResult;
};
#endif
#ifdef GPGPU_JSON_SERIALISATION_SUPPORT
#include "inline/ecs/JsonSerialisers.hpp"
#endif
#ifdef GPGPU_YML_SERIALISATION_SUPPORT
#include "inline/ecs/YmlSerialisers.hpp"
#endif

} // namespace ECS
