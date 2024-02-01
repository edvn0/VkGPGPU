#include "pch/vkgpgpu_pch.hpp"

#include "ecs/serialisation/SceneSerialiser.hpp"

namespace ECS {

void SceneSerialiser::serialise(const Scene &scene, std::ostream &stream) {
  auto &registry = scene.get_registry();
  auto t0 = std::chrono::high_resolution_clock::now();
  registry.view<IdentityComponent>().each([&](const auto entity, auto &) {
    serialise_entity_components(registry, stream, entity);
  });
  auto t1 = std::chrono::high_resolution_clock::now();

  info("Serialised scene in {}ms",
       std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

void SceneSerialiser::deserialise(Scene &scene, std::istream &stream) {
  if (!stream) {
    throw std::runtime_error("Unable to open stream for reading");
  }
  auto &registry = scene.get_registry();
  while (stream.peek() != EOF) {
    const entt::entity entity = registry.create();
    registry.emplace<IdentityComponent>(entity);
    registry.emplace<TransformComponent>(entity);
    deserialise_entity_components(registry, stream, entity);
  }
}

} // namespace ECS
