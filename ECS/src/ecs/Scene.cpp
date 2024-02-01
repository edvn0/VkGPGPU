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
      [&](auto, auto &transform, auto &camera) {
        transform.position = glm::inverse(view_matrix)[3];
      });

  scene_renderer.begin_frame(projection_matrix, view_matrix);
  scene_renderer.flush();
  scene_renderer.end_frame();
}

auto Scene::on_create(const Core::Device &device, const Core::Window &,
                      const Core::Swapchain &) -> void {

  auto camera_entity = create_entity("Camera");
  camera_entity.add_component<CameraComponent>();
  camera_entity.add_component<MeshComponent>(
      Core::Mesh::reference_import_from(device, Core::FS::model("sphere.fbx")));

  constexpr auto L = 30;
  constexpr auto l = 2;
  constexpr auto m = 2;
  constexpr auto num_cubes_per_side = L / (l + m);

  constexpr glm::vec4 colorStart(1.0f, 0.0f, 0.0f, 1.0f); // Red
  constexpr glm::vec4 colorEnd(0.0f, 0.0f, 1.0f, 1.0f);   // Blue

  float total_size = num_cubes_per_side * l + (num_cubes_per_side - 1) * m;

  for (int x = 0; x < num_cubes_per_side; ++x) {
    for (int y = 0; y < num_cubes_per_side; ++y) {
      for (int z = 0; z < num_cubes_per_side; ++z) {
        // Calculate the position of the current subcube
        float posX = x * (l + m) - total_size / 2 + l / 2;
        float posY = y * (l + m) - total_size / 2 + l / 2;
        float posZ = z * (l + m) - total_size / 2 + l / 2;

        // Create entities
        auto entity = create_entity(fmt::format("Subcube-{}-{}-{}", x, y, z));

        // Create components
        auto &transform = entity.add_component<TransformComponent>();
        auto &mesh = entity.add_component<MeshComponent>();
        auto &texture = entity.add_component<TextureComponent>();

        mesh.mesh = Core::Mesh::reference_import_from(
            device, Core::FS::model("cube.fbx"));
        float ratio_x = static_cast<float>(x) / (num_cubes_per_side - 1);
        float ratio_y = static_cast<float>(y) / (num_cubes_per_side - 1);
        float ratio_z = static_cast<float>(z) / (num_cubes_per_side - 1);
        const float mix_ratio = (ratio_x + ratio_y + ratio_z) / 3.0f;
        texture.colour = glm::mix(colorStart, colorEnd, mix_ratio);

        transform.position = glm::vec3{posX, posY, posZ};
      }
    }
  }

  constexpr glm::vec4 otherColorStart(0.1F, 0.1f, 0.9f, 1.0f); // Red
  constexpr glm::vec4 otherColorEnd(0.0f, 0.9f, 1.0f, 1.0f);   // Blue
  for (int x = 0; x < num_cubes_per_side; ++x) {
    for (int y = 0; y < num_cubes_per_side; ++y) {
      for (int z = 0; z < num_cubes_per_side; ++z) {
        // Calculate the position of the current subcube
        float posX = total_size + x * (l + m) - total_size / 2 + l / 2;
        float posY = total_size + y * (l + m) - total_size / 2 + l / 2;
        float posZ = total_size + z * (l + m) - total_size / 2 + l / 2;

        // Create entities
        auto entity = create_entity(fmt::format("Subsphere-{}-{}-{}", x, y, z));

        // Create components
        auto &transform = entity.add_component<TransformComponent>();
        auto &mesh = entity.add_component<MeshComponent>();
        auto &texture = entity.add_component<TextureComponent>();

        mesh.mesh = Core::Mesh::reference_import_from(
            device, Core::FS::model("sphere.fbx"));
        float ratio_x = static_cast<float>(x) / (num_cubes_per_side - 1);
        float ratio_y = static_cast<float>(y) / (num_cubes_per_side - 1);
        float ratio_z = static_cast<float>(z) / (num_cubes_per_side - 1);
        const float mix_ratio = (ratio_x + ratio_y + ratio_z) / 3.0f;
        texture.colour = glm::mix(otherColorStart, otherColorEnd, mix_ratio);

        transform.position = glm::vec3{posX, posY, posZ};
      }
    }
  }
}

auto Scene::temp_on_create(const Core::Device &device, const Core::Window &,
                           const Core::Swapchain &) -> void {
  registry.view<MeshComponent>().each([&](auto, MeshComponent &mesh) {
    mesh.mesh = Core::Mesh::reference_import_from(device, mesh.path);
  });
}

auto Scene::on_destroy() -> void {
  for (auto *observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

} // namespace ECS
