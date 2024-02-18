#pragma once

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
      throw std::runtime_error("Unable to open file for writing: " +
                               Core::FS::Path{filename}.string());
    }
    serialise(scene, out_stream);
  }

  void deserialise(Scene &scene, const Core::StringLike auto &filename) {
    std::ifstream in_stream(Core::FS::Path{filename}, std::ios::binary);
    if (!in_stream.is_open()) {
      throw std::runtime_error("Unable to open file for reading: " +
                               Core::FS::Path{filename}.string());
    }
    deserialise(scene, in_stream);
  }

  auto serialise(const Scene &scene, std::ostream &stream) -> void;
  auto deserialise(Scene &scene, std::istream &stream) -> void;

private:
  using ComponentTypes = Core::TypelistToTuple<EngineComponents>::type;
  static constexpr auto ComponentCount = std::tuple_size_v<ComponentTypes>;

  auto serialise_entity_components(std::ostream &out, Entity &entity) -> bool;
  auto deserialise_entity_components(std::istream &in, Entity &entity) -> bool;

  template <std::size_t... Is>
  auto make_component_mask(Entity &entity, std::index_sequence<Is...>) {
    Core::u32 mask = 0;
    ((entity.any_of<std::tuple_element_t<Is, ComponentTypes>>()
          ? mask |= (1 << Is)
          : mask),
     ...);
    return mask;
  }

  template <typename T>
  auto serialise_component(std::ostream &out, Entity &entity) -> bool {
    if (entity.has_component<T>()) {
      const auto &component = entity.get_component<T>();
      if (!ComponentSerialiser<T>::serialise(component, out)) {
        error("Failed to serialise component of type {}", typeid(T).name());
        return false;
      }
    }
    return true;
  }

  template <std::size_t... Is>
  auto serialise_entity_components_impl(std::ostream &out, Entity &entity,
                                        std::index_sequence<Is...>) -> bool {
    // Write component mask
    auto mask = make_component_mask(entity, std::index_sequence<Is...>{});
    out.write(std::bit_cast<const char *>(&mask), sizeof(mask));

    // Serialize components based on mask
    return (serialise_component<std::tuple_element_t<Is, ComponentTypes>>(
                out, entity) &&
            ...);
  }

  template <typename T>
  auto deserialise_component(std::istream &in, Entity &entity, Core::u32 mask,
                             Core::u32 component_bit) -> bool {
    if (mask & component_bit) {
      auto &t = entity.add_component<T>();
      if (!ComponentSerialiser<T>::deserialise(in, t)) {
        error("Failed to deserialise component of type {}", typeid(T).name());
        return false;
      }
    }
    return true;
  }

  template <std::size_t... Is>
  auto deserialise_entity_components_impl(std::istream &in, Entity &entity,
                                          std::index_sequence<Is...>) -> bool {
    Core::u32 mask = 0;
    in.read(std::bit_cast<char *>(&mask), sizeof(mask));

    // Deserialise components based on mask. Stop if any deserialization fails.
    return (deserialise_component<std::tuple_element_t<Is, ComponentTypes>>(
                in, entity, mask, (1 << Is)) &&
            ...);
  }
};

} // namespace ECS
