#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Scene.hpp"

#include "Formatters.hpp"
#include "Logger.hpp"
#include "SceneRenderer.hpp"

#include <entt/entt.hpp>
#include <execution>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp"

namespace ECS {

Scene::Scene(std::string_view scene_name) : name{scene_name} {}

Scene::~Scene() {
  SceneSerialiser serialiser;
  serialiser.serialise(*this, Core::FS::Path{name + ".scene"});
}

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
  const auto camera_view =
      registry.view<const TransformComponent, const MeshComponent,
                    const CameraComponent>();
  camera_view.each(
      [&renderer](auto, auto &&transform, auto &&mesh, auto &&camera) {
        renderer.submit_static_mesh(mesh.mesh.get(), transform.compute(),
                                    glm::vec4{camera.field_of_view / 90.0F,
                                              camera.field_of_view / 90.0F,
                                              0.0F, 1.0F});
      });

  const auto other_view =
      registry.view<const TransformComponent, const MeshComponent,
                    const TextureComponent>();
  other_view.each(
      [&renderer](auto, auto &&transform, auto &&mesh, auto &&texture) {
        renderer.submit_static_mesh(mesh.mesh.get(), transform.compute(),
                                    texture.colour);
      });
}

auto Scene::on_render(Core::SceneRenderer &scene_renderer, Core::floating ts,
                      const glm::mat4 &projection_matrix,
                      const glm::mat4 &view_matrix) -> void {
  registry.view<TransformComponent, CameraComponent>().each(
      [&](auto, auto &transform, auto &) {
        transform.position = glm::inverse(view_matrix)[3];
      });

  scene_renderer.begin_frame(projection_matrix, view_matrix);
  scene_renderer.flush();
  scene_renderer.end_frame();
}

auto Scene::on_create(const Core::Device &device, const Core::Window &,
                      const Core::Swapchain &) -> void {
  // Loop over all Meshes and create them, if the path is not empty
  // We do this to ensure that deserialisers won't have to care about Devices
  registry.view<MeshComponent>().each([&](auto, auto &mesh) {
    if (!mesh.mesh && !mesh.path.empty()) {
      mesh.mesh = Core::Mesh::reference_import_from(device, mesh.path);
    }
  });

  auto camera_entity = create_entity("Camera");
  camera_entity.add_component<CameraComponent>();
  camera_entity.add_component<MeshComponent>(
      Core::Mesh::reference_import_from(device, Core::FS::model("sphere.fbx")));

  auto basic_cube_at_3_3_1 = create_entity("Basic Cube at 3, 3, 1");
  auto &transform = basic_cube_at_3_3_1.add_component<TransformComponent>();
  transform.position = glm::vec3{3.0F, 3.0F, 1.0F};
  basic_cube_at_3_3_1.add_component<MeshComponent>(
      Core::Mesh::reference_import_from(device, Core::FS::model("cube.fbx")));
  basic_cube_at_3_3_1.add_component<TextureComponent>(
      glm::vec4{1.0F, 0.0F, 1.0F, 1.0F});
}

auto Scene::on_destroy() -> void {
  for (auto *observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

} // namespace ECS
