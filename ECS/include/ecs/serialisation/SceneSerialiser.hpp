#pragma once

#include <fstream>
#include <string>
#include <string_view>

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
  using ComponentTypes =
      std::tuple<IdentityComponent, TransformComponent, TextureComponent,
                 MeshComponent, CameraComponent>;
  static constexpr auto ComponentCount = std::tuple_size_v<ComponentTypes>;

  template <std::size_t... Is>
  auto make_component_mask(const entt::registry &registry, entt::entity entity,
                           std::index_sequence<Is...>) {
    Core::u32 mask = 0;
    ((registry.any_of<std::tuple_element_t<Is, ComponentTypes>>(entity)
          ? mask |= (1 << Is)
          : mask),
     ...);
    return mask;
  }

  template <typename T>
  void serialise_component(const entt::registry &registry, std::ostream &out,
                           entt::entity entity) {
    if (registry.any_of<T>(entity)) {
      const auto &component = registry.get<T>(entity);
      ComponentSerialiser<T>::serialise(component, out);
    }
  }

  template <std::size_t... Is>
  void serialise_entity_components_impl(const entt::registry &registry,
                                        std::ostream &out, entt::entity entity,
                                        std::index_sequence<Is...>) {
    // Write component mask
    auto mask =
        make_component_mask(registry, entity, std::index_sequence<Is...>{});
    out.write(reinterpret_cast<const char *>(&mask), sizeof(mask));

    // Serialize components based on mask
    (serialise_component<std::tuple_element_t<Is, ComponentTypes>>(registry,
                                                                   out, entity),
     ...);
  }

  void serialise_entity_components(const entt::registry &registry,
                                   std::ostream &out, entt::entity entity) {
    serialise_entity_components_impl(
        registry, out, entity, std::make_index_sequence<ComponentCount>{});
  }

  template <typename T>
  void deserialise_component(entt::registry &registry, std::istream &in,
                             entt::entity entity, Core::u32 mask,
                             Core::u32 component_bit) {
    if (mask & component_bit) {
      // const auto& old = registry.get<T>(entity);
      auto component = ComponentSerialiser<T>::deserialise(in);
      registry.emplace_or_replace<T>(entity, component);
    }
  }

  template <std::size_t... Is>
  void deserialise_entity_components_impl(entt::registry &registry,
                                          std::istream &in, entt::entity entity,
                                          std::index_sequence<Is...>) {
    Core::u32 mask;
    in.read(reinterpret_cast<char *>(&mask), sizeof(mask));

    // Deserialise components based on mask
    (deserialise_component<std::tuple_element_t<Is, ComponentTypes>>(
         registry, in, entity, mask, (1 << Is)),
     ...);
  }

  void deserialise_entity_components(entt::registry &registry, std::istream &in,
                                     entt::entity entity) {
    deserialise_entity_components_impl(
        registry, in, entity, std::make_index_sequence<ComponentCount>{});
  }
};

} // namespace ECS
