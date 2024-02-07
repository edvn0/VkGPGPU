#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Scene.hpp"

#include "Colours.hpp"
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
  try {
    serialiser.serialise(*this, Core::FS::Path{name + ".scene"});
  } catch (const std::exception &e) {
    error("Failed to serialise scene: {}", e.what());
  }
  info("Scene {} destroyed and serialised.", name);
}

auto Scene::create_entity(const std::string_view entity_name, bool add_observer)
    -> Entity {
  Entity entity{this, entity_name};
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
  camera_view.each([&renderer](auto, const auto &transform, const auto &mesh,
                               const auto &camera) {
    renderer.submit_static_mesh(mesh.mesh.get(), transform.compute());
  });

  const auto other_view =
      registry.view<const TransformComponent, const MeshComponent>();
  other_view.each([&renderer](auto, const auto &transform, const auto &mesh) {
    renderer.submit_static_mesh(mesh.mesh.get(), transform.compute());
  });
}

auto Scene::get_entity(entt::entity identifier) -> std::optional<Entity> {
  Entity entity{this, identifier};
  return entity;
};

auto Scene::get_entity(Core::u64 identifier) -> std::optional<Entity> {
  std::optional<Entity> entity = std::nullopt;
  registry.view<IdentityComponent>().each([&](auto entt, auto &identity) {
    if (identity.id == identifier) {
      entity = Entity{this, entt};
    }
  });

  return entity;
}

auto Scene::on_render(Core::SceneRenderer &scene_renderer, Core::floating ts,
                      const glm::mat4 &projection_matrix,
                      const glm::mat4 &view_matrix) -> void {
  registry.view<TransformComponent, CameraComponent>().each(
      [&](auto, auto &transform, auto &) {
        transform.position = glm::inverse(view_matrix)[3];
      });

  const auto sun_view =
      registry.view<const SunComponent, const TransformComponent>();
  sun_view.each([&](auto, const auto &sun, const auto &transform) {
    scene_renderer.get_sun_position() = transform.position;
    scene_renderer.get_depth_factors() = sun.depth_params;

    auto &renderer_config = scene_renderer.get_renderer_configuration();
    renderer_config.view = view_matrix;
    renderer_config.projection = projection_matrix;
    renderer_config.view_projection = projection_matrix * view_matrix;
    renderer_config.light_position = glm::vec4{transform.position, 1.0F};
    renderer_config.light_direction =
        glm::normalize(-glm::vec4{sun.direction, 1.0F});
    renderer_config.camera_position = glm::inverse(view_matrix)[3];
    renderer_config.light_colour = sun.colour;
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

  auto sun = create_entity("Sun");
  auto &sun_component = sun.add_component<SunComponent>();
  sun_component.colour = glm::vec4{1.0F, 1.0F, 1.0F, 1.0F};
  sun_component.direction = glm::vec3{0.0F, 0.0F, 1.0F};
  sun.get_transform().position = glm::vec3{-3.0F, -4.0F, 2.0F};
  sun.add_component<MeshComponent>(
      Core::Mesh::reference_import_from(device, Core::FS::model("sphere.fbx")));

  auto camera_entity = create_entity("Camera");
  camera_entity.add_component<CameraComponent>();

#if 0
  auto basic_cube_at_3_3_1 = create_entity("Basic Cube at 3, 3, 1");
  auto &transform = basic_cube_at_3_3_1.add_component<TransformComponent>();
  transform.position = glm::vec3{3.0F, 3.0F, 1.0F};
  transform.scale = glm::vec3{2.F, 2.0F, 2.0F};
  basic_cube_at_3_3_1.add_component<MeshComponent>(
      Core::Mesh::reference_import_from(device, Core::FS::model("cube.fbx")));
  basic_cube_at_3_3_1.add_component<TextureComponent>(
      glm::vec4{1.0F, 0.0F, 1.0F, 1.0F});

  auto meta_parent = create_entity("MetaParent");
  for (auto i = 0; i < 10; i++) {
    auto metahuman = create_entity(fmt::format("Metahuman.{}", i));

    metahuman.set_parent(meta_parent);

    auto &metahuman_transform = metahuman.add_component<TransformComponent>();
    // Random glm::vec3
    const auto random_position =
        glm::vec3{static_cast<Core::floating>(rand() % 10) - 5,
                  static_cast<Core::floating>(rand() % 10) - 5,
                  static_cast<Core::floating>(rand() % 10) - 5};

    metahuman_transform.position = random_position;

    metahuman.add_component<MeshComponent>(Core::Mesh::reference_import_from(
        device, Core::FS::model("metahuman/metahuman.fbx")));
    metahuman.add_component<TextureComponent>(
        glm::vec4{1.0F, 1.0F, 1.0F, 1.0F});
    metahuman_transform.rotation =
        glm::rotate(metahuman_transform.rotation, glm::radians(90.0F),
                    glm::vec3{1.0F, 0.0F, 0.0F});
    metahuman_transform.scale = glm::vec3{0.01F, 0.01F, 0.01F};
  }
#endif

  auto cerberus_model = create_entity("Cerberus Model");
  auto &cerberus_transform = cerberus_model.add_component<TransformComponent>();
  cerberus_transform.position = glm::vec3{-2.F, 0.0F, 2.F};
  cerberus_transform.scale = glm::vec3{0.3F, 0.3F, 0.3F};
  cerberus_transform.rotation =
      glm::rotate(cerberus_transform.rotation, glm::radians(90.0F),
                  glm::vec3{1.0F, 0.0F, 0.0F});
  cerberus_model.add_component<MeshComponent>(Core::Mesh::reference_import_from(
      device, Core::FS::model("cerberus/cerberus.fbx")));
}

auto Scene::on_destroy() -> void {
  for (auto *observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

} // namespace ECS
