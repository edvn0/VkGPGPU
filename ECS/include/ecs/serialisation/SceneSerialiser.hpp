#pragma once

#include "Exception.hpp"

#include <fstream>
#include <string>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Scene.hpp"
#include "ecs/serialisation/Serialisers.hpp"

namespace ECS {

class SceneSerialiser {
public:
  // Serializes the entire scene to a binary file
  void serialise(const Scene &scene, const Core::StringLike auto &filename) {
    std::ofstream out_stream(Core::FS::Path{filename}, std::ios::binary);
    if (!out_stream.is_open()) {
      throw Core::UnableToOpenException("Unable to open file for writing: " +
                                        Core::FS::Path{filename}.string());
    }
    serialise(scene, out_stream);
  }

  void deserialise(Scene &scene, const Core::StringLike auto &filename) {
    std::ifstream in_stream(Core::FS::Path{filename}, std::ios::binary);
    if (!in_stream.is_open()) {
      throw Core::UnableToOpenException("Unable to open file for reading: " +
                                        Core::FS::Path{filename}.string());
    }
    deserialise(scene, in_stream);
  }

  auto serialise(const Scene &scene, std::ostream &stream) -> void;
  auto deserialise(Scene &scene, std::istream &stream) -> void;

  auto serialise_entity_components(std::ostream &out,
                                   const ImmutableEntity &entity) -> bool;
  auto deserialise_entity_components(std::istream &in, Entity &entity) -> bool;

private:
  using ComponentTypes = Core::TypelistToTuple<EngineComponents>::type;
  static constexpr auto ComponentCount = std::tuple_size_v<ComponentTypes>;

  template <std::size_t... Is>
  auto make_component_mask(Entity &entity, std::index_sequence<Is...>) {
    Core::u32 mask = 0;
    ((entity.any_of<std::tuple_element_t<Is, ComponentTypes>>()
          ? mask |= (1 << Is)
          : mask),
     ...);
    return mask;
  }

  template <std::size_t... Is>
  auto make_component_mask(const ImmutableEntity &entity,
                           std::index_sequence<Is...>) {
    Core::u32 mask = 0;
    ((entity.any_of<std::tuple_element_t<Is, ComponentTypes>>()
          ? mask |= (1 << Is)
          : mask),
     ...);
    return mask;
  }

  template <SerialisationType TS, typename T>
  auto serialise_component(std::ostream &out, const ImmutableEntity &entity)
      -> bool {
    if (entity.has_component<T>()) {
      const auto &component = entity.get_component<T>();
      if (const auto result =
              ComponentSerialiser<TS, T>::serialise(component, out);
          !result) {
        error("Failed to serialise component of type {}. Reason: {}",
              typeid(T).name(), result.reason);
        return false;
      }
    }
    return true;
  }

  template <SerialisationType TS, std::size_t... Is>
  auto serialise_entity_components_impl(std::ostream &out,
                                        const ImmutableEntity &entity,
                                        std::index_sequence<Is...>) -> bool {
    // Write component mask
    auto mask = make_component_mask(entity, std::index_sequence<Is...>{});
    out.write(std::bit_cast<const char *>(&mask), sizeof(mask));

    // Serialize components based on mask
    return (serialise_component<TS, std::tuple_element_t<Is, ComponentTypes>>(
                out, entity) &&
            ...);
  }

  template <SerialisationType TS, typename T>
  auto deserialise_component(std::istream &in, Entity &entity, Core::u32 mask,
                             Core::u32 component_bit) -> bool {
    if (mask & component_bit) {
      auto &t = entity.add_component<T>();
      if (const auto result = ComponentSerialiser<TS, T>::deserialise(in, t);
          !result) {
        error("Failed to deserialise component of type {}. Reason: {}",
              typeid(T).name(), result.reason);
        return false;
      }
    }
    return true;
  }

  template <SerialisationType TS, std::size_t... Is>
  auto deserialise_entity_components_impl(std::istream &in, Entity &entity,
                                          std::index_sequence<Is...>) -> bool {
    Core::u32 mask = 0;
    in.read(std::bit_cast<char *>(&mask), sizeof(mask));

    // Deserialise components based on mask. Stop if any deserialization fails.
    return (deserialise_component<TS, std::tuple_element_t<Is, ComponentTypes>>(
                in, entity, mask, (1 << Is)) &&
            ...);
  }
};

} // namespace ECS
