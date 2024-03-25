#include "pch/vkgpgpu_pch.hpp"

#include "ecs/serialisation/SceneSerialiser.hpp"

#include "ecs/Entity.hpp"

namespace ECS {

void SceneSerialiser::serialise(const Scene &scene, std::ostream &stream) {
  if (!stream) {
    throw Core::UnableToOpenException("Unable to open stream for writing");
  }

  auto &registry = scene.get_registry();
  const auto t0 = std::chrono::high_resolution_clock::now();
  registry.view<IdentityComponent>().each([&](const auto entity,
                                              auto &identity) {
    auto scene_entity =
        ImmutableEntity{&scene, entity}; // Entity{const_cast<Scene *>(&scene),
                                         // entity, identity.name};
    if (!serialise_entity_components(stream, scene_entity)) {
    }
  });
  const auto t1 = std::chrono::high_resolution_clock::now();

  info("Serialised scene in {}ms",
       std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

void SceneSerialiser::deserialise(Scene &scene, std::istream &stream) {
  if (!stream) {
    throw Core::UnableToOpenException("Unable to open stream for reading");
  }

  const auto t0 = std::chrono::high_resolution_clock::now();
  bool success = true;
  auto &registry = scene.get_registry();
  while (stream.peek() != EOF) {
    Entity entity{&scene, registry.create(), "Empty"};
    if (!deserialise_entity_components(stream, entity)) {
      success = false;
      break;
    }
  }
  const auto t1 = std::chrono::high_resolution_clock::now();

  if (success) {
    info(
        "Deserialised scene in {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
  } else {
    warn(
        "Could not deserialise. Failed after {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
  }
}

auto SceneSerialiser::serialise_entity_components(std::ostream &out,
                                                  const ImmutableEntity &entity)
    -> bool {
  return serialise_entity_components_impl<SerialisationType::Binary>(
      out, entity, std::make_index_sequence<ComponentCount>{});
}

auto SceneSerialiser::deserialise_entity_components(std::istream &in,
                                                    Entity &entity) -> bool {
  return deserialise_entity_components_impl<SerialisationType::Binary>(
      in, entity, std::make_index_sequence<ComponentCount>{});
}

} // namespace ECS
