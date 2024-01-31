#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Scene.hpp"

#include "Formatters.hpp"
#include "Logger.hpp"
#include "SceneRenderer.hpp"

#include <entt/entt.hpp>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

Scene::Scene(std::string_view scene_name) : name{scene_name} {}

Scene::~Scene() = default;

auto Scene::create_entity(const std::string_view entity_name, bool add_observer)
    -> Entity {
  Entity entity{this, entity_name};
#if 0
  for (auto observer : observers) {
    observer->on_notify(Events::EntityAddedEvent{
        .id = entity.get_id(),
    });
  }

  if (add_observer) {
    observers.push_back(&entity);
  }
#endif
  return entity;
}

auto Scene::delete_entity(const Core::u64 identifier) -> void {

  registry.view<IdentityComponent>().each([&](auto entity, auto &identity) {
    if (identity.id == identifier) {
      registry.destroy(entity);
    }
  });

  for (auto observer : observers) {
    observer->on_notify(Events::EntityRemovedEvent{
        .id = identifier,
    });
  }
}

auto Scene::on_update(Core::SceneRenderer &renderer, Core::floating dt)
    -> void {
  for (auto &&[entity, transform, mesh, texture] :
       registry
           .view<const TransformComponent, const MeshComponent,
                 const TextureComponent>()
           .each()) {
    renderer.submit_static_mesh(mesh.mesh.get(), transform.compute(),
                                texture.colour);
  }
}

auto Scene::on_render(Core::SceneRenderer &scene_renderer, Core::floating ts,
                      const Core::Math::Vec3 &camera_position) -> void {
  // Scene renderer keeps track of current frame.
  scene_renderer.begin_frame(camera_position);
  scene_renderer.flush();
  scene_renderer.end_frame();
}

auto Scene::on_create(const Core::Device &device, const Core::Window &,
                      const Core::Swapchain &) -> void {
  constexpr auto thirty = 30;
  for (const auto x : std::views::iota(0, thirty)) {
    for (const auto y : std::views::iota(0, thirty)) {
      auto entity = create_entity(fmt::format("Entity {} {}", x, y));
      auto &[pos, rot, scl] = entity.add_component<TransformComponent>();
      pos = glm::vec3(x, y, (x + y));
      auto cube_or_sphere = (x + y) % 2 == 0 ? "cube.fbx" : "sphere.fbx";
      (void)entity.add_component<MeshComponent>(
          Core::Mesh::reference_import_from(device,
                                            Core::FS::model(cube_or_sphere)));
      auto &[colour] = entity.add_component<TextureComponent>();
      const auto gradient = glm::vec4(
          static_cast<Core::floating>(x) / static_cast<Core::floating>(thirty),
          static_cast<Core::floating>(y) / static_cast<Core::floating>(thirty),
          0.0F, 1.0F);
      colour = gradient;
    }
  }
}

auto Scene::on_destroy() -> void {
  for (auto observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

} // namespace ECS
