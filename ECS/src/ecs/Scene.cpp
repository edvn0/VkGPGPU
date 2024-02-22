#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Scene.hpp"

#include "Colours.hpp"
#include "Formatters.hpp"
#include "Logger.hpp"
#include "SceneRenderer.hpp"
#include "UI.hpp"

#include <entt/entt.hpp>
#include <execution>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp"

namespace ECS {

static Core::AABB
get_aabb_for_geometry(const BasicGeometry::GeometryVariant &geom,
                      const glm::mat4 &transform) {
  Core::AABB aabb;

  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        std::vector<glm::vec3>
            points; // Used for geometries defined by vertices

        if constexpr (std::is_same_v<T, BasicGeometry::QuadParameters>) {
          // Assuming Quad is centered at origin; adjust if not
          float halfWidth = arg.width / 2.0f;
          float halfHeight = arg.height / 2.0f;
          points = {{-halfWidth, -halfHeight, 0.0f},
                    {halfWidth, halfHeight, 0.0f}};
        } else if constexpr (std::is_same_v<
                                 T, BasicGeometry::TriangleParameters>) {
          // Assuming Triangle is equilateral and centered at origin; adjust if
          // not
          float halfBase = arg.base / 2.0f;
          points = {{-halfBase, 0.0f, 0.0f},
                    {halfBase, 0.0f, 0.0f},
                    {0.0f, arg.height, 0.0f}};
        } else if constexpr (std::is_same_v<T,
                                            BasicGeometry::CircleParameters> ||
                             std::is_same_v<T,
                                            BasicGeometry::SphereParameters>) {
          // For Circle/Sphere, adjust AABB based on radius and scale; assuming
          // centered at origin
          glm::vec3 scaleFactors =
              glm::vec3(transform[0][0], transform[1][1], transform[2][2]) *
              arg.radius;
          glm::vec3 center =
              glm::vec3(transform[3]); // Translation component of the matrix
          glm::vec3 min = center - scaleFactors;
          glm::vec3 max = center + scaleFactors;
          aabb.update(min, max);
          return; // Early return as we've already calculated AABB
        } else if constexpr (std::is_same_v<T, BasicGeometry::CubeParameters>) {
          float halfSide = arg.side_length / 2.0f;
          points = {{-halfSide, -halfSide, -halfSide},
                    {halfSide, halfSide, halfSide}};
        }

        // Transform points and update AABB for non-spherical objects
        for (auto &point : points) {
          glm::vec4 worldPoint = transform * glm::vec4(point, 1.0f);
          aabb.update(glm::vec3(worldPoint));
        }
      },
      geom);

  return aabb;
}

static std::tuple<glm::vec3, float, glm::vec3, glm::vec3>
compute_scene_center_and_radius(Scene &scene) {
  glm::vec3 scene_min(std::numeric_limits<float>::max());
  glm::vec3 scene_max(std::numeric_limits<float>::lowest());

  auto &registry = scene.get_registry();
  // Process mesh components
  for (const auto &[entity, transform, mesh] :
       registry
           .view<const TransformComponent, const MeshComponent>(
               entt::exclude<SunComponent, CameraComponent>)
           .each()) {
    if (!mesh.mesh)
      continue;

    const auto &aabb = mesh.mesh->get_aabb();
    glm::vec3 min_corners = transform.compute() * aabb.min_vector();
    glm::vec3 max_corners = transform.compute() * aabb.max_vector();

    scene_min = glm::min(scene_min, min_corners);
    scene_max = glm::max(scene_max, max_corners);
  }

  // Process geometry components
  for (const auto &[entity, transform, geometry] :
       registry.view<const TransformComponent, const GeometryComponent>()
           .each()) {
    // Hypothetical function to get AABB for geometry
    auto aabb = get_aabb_for_geometry(geometry.parameters, transform.compute());
    glm::vec3 min_corners = aabb.min(); // Assuming first is min_vector
    glm::vec3 max_corners = aabb.max(); // Assuming second is max_vector

    scene_min = glm::min(scene_min, min_corners);
    scene_max = glm::max(scene_max, max_corners);
  }

  glm::vec3 center = (scene_min + scene_max) / 2.0f;
  float radius = glm::distance(scene_min, scene_max) / 2.0f;

  return {center, radius, scene_min, scene_max};
}

static glm::mat4 compute_light_view_matrix(const glm::vec3 &light_position,
                                           const glm::vec3 &scene_center) {
  return glm::lookAt(
      light_position, scene_center,
      glm::vec3(0, -1, 0)); // Assuming Y-up in your coordinate system
}

static std::pair<glm::vec3, glm::vec3>
compute_light_space_bounds(const glm::mat4 &light_view_matrix, Scene &scene) {
  glm::vec3 scene_min(std::numeric_limits<float>::max());
  glm::vec3 scene_max(std::numeric_limits<float>::lowest());

  for (const auto &[entity, transform, mesh] :
       scene.get_registry()
           .view<const TransformComponent, const MeshComponent>(
               entt::exclude<SunComponent, CameraComponent>)
           .each()) {
    if (!mesh.mesh)
      continue;

    const auto &aabb = mesh.mesh->get_aabb();
    std::array<glm::vec4, 8> corners = {{
        // Define all 8 corners of the AABB in local space
        glm::vec4(aabb.min().x, aabb.min().y, aabb.min().z, 1.0),
        glm::vec4(aabb.min().x, aabb.min().y, aabb.max().z, 1.0),
        glm::vec4(aabb.min().x, aabb.max().y, aabb.min().z, 1.0),
        glm::vec4(aabb.min().x, aabb.max().y, aabb.max().z, 1.0),
        glm::vec4(aabb.max().x, aabb.min().y, aabb.min().z, 1.0),
        glm::vec4(aabb.max().x, aabb.min().y, aabb.max().z, 1.0),
        glm::vec4(aabb.max().x, aabb.max().y, aabb.min().z, 1.0),
        glm::vec4(aabb.max().x, aabb.max().y, aabb.max().z, 1.0),
    }};

    glm::mat4 modelMatrix =
        glm::translate(glm::mat4(1.0), transform.position) *
        glm::scale(glm::mat4(1.0),
                   transform.scale); // Assuming rotation is part of
                                     // transform.position for simplicity

    for (auto &corner : corners) {
      glm::vec4 transformedCorner = light_view_matrix * modelMatrix * corner;
      scene_min = glm::min(scene_min,
                           glm::vec3(transformedCorner) / transformedCorner.w);
      scene_max = glm::max(scene_max,
                           glm::vec3(transformedCorner) / transformedCorner.w);
    }
  }

  return {scene_min, scene_max};
}

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

auto Scene::delete_entity(const Core::u64 identifier) -> bool {
  auto view = registry.view<IdentityComponent>();
  entt::entity found = entt::null;
  view.each([&](auto entity, auto &identity) {
    if (identity.id == identifier) {
      found = entity;
    }
  });

  if (found == entt::null) {
    Core::UI::Toast::error(3000, "Invalid entity {} tried to be deleted.",
                           identifier);
    return false;
  }

  registry.destroy(found);

  for (auto observer : observers) {
    observer->on_notify(Events::EntityRemovedEvent{
        .id = identifier,
    });
  }

  sort();

  return true;
}

auto Scene::on_update(Core::SceneRenderer &renderer, Core::floating dt)
    -> void {
  while (!futures.empty()) {
    auto &future = futures.front();
    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      future.get();
      futures.pop();
    } else {
      break;
    }
  }

  const auto geometry_view_without_camera_and_texture =
      registry.view<const TransformComponent, const MeshComponent>(
          entt::exclude<CameraComponent, TextureComponent>);
  geometry_view_without_camera_and_texture.each(
      [&renderer](auto, const auto &transform, const MeshComponent &mesh) {
        if (mesh.mesh != nullptr) {
          renderer.submit_static_mesh(mesh.mesh.get(), transform.compute());
        }
      });

  const auto geometry_view_without_camera_including_texture =
      registry.view<const TransformComponent, const MeshComponent,
                    const TextureComponent>(entt::exclude<CameraComponent>);
  geometry_view_without_camera_including_texture.each(
      [&renderer](auto, const auto &transform, const MeshComponent &mesh,
                  const TextureComponent &text) {
        if (mesh.mesh != nullptr) {
          renderer.submit_static_mesh(mesh.mesh.get(), transform.compute(),
                                      text.colour);
        }
      });

  const auto basic_geometry_view =
      registry.view<const TransformComponent, const GeometryComponent,
                    const TextureComponent>();
  basic_geometry_view.each([&renderer](auto, const auto &transform,
                                       const GeometryComponent &geom,
                                       const TextureComponent &text) {
    const auto computed = transform.compute();
    const auto &col = text.colour;

    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, ECS::BasicGeometry::CubeParameters>) {
            renderer.submit_cube(computed, col);
          }
        },
        geom.parameters);
  });
}

auto Scene::get_entity(entt::entity identifier) -> std::optional<Entity> {
  if (!registry.valid(identifier))
    return std::nullopt;

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

auto Scene::sort() -> void {
  return registry.sort<IdentityComponent>(
      [](const IdentityComponent &lhs, const IdentityComponent &rhs) {
        return std::ranges::lexicographical_compare(lhs.name, rhs.name);
      });
}

void Scene::initialise_device_dependent_objects(const Core::Device &device) {
  registry.view<MeshComponent>().each([&](auto, auto &mesh) {
    if (!mesh.mesh && !mesh.path.empty()) {
      mesh.mesh = Core::Mesh::reference_import_from(device, mesh.path);
    }
  });
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
  sun_view.each([&](auto, const SunComponent &sun, const auto &transform) {
    // Update the scene renderer with the sun's position and depth factors
    scene_renderer.get_sun_position() = transform.position;
    scene_renderer.get_depth_factors() = sun.depth_params;

    // Update renderer configuration
    auto &renderer_config = scene_renderer.get_renderer_configuration();
    renderer_config.view = view_matrix;
    renderer_config.projection = projection_matrix;
    renderer_config.view_projection = projection_matrix * view_matrix;
    renderer_config.inverse_view = glm::inverse(view_matrix);
    renderer_config.inverse_projection = glm::inverse(projection_matrix);
    renderer_config.inverse_view_projection =
        renderer_config.inverse_view * renderer_config.inverse_projection;
    renderer_config.light_position = glm::vec4{transform.position, 1.0F};

    // Step 1: Compute the Scene's Center and Radius
    auto &&[scene_center, scene_radius, scene_min, scene_max] =
        compute_scene_center_and_radius(*this);

    // Update light direction based on the scene center
    renderer_config.light_direction = -glm::normalize(
        renderer_config.light_position - glm::vec4{scene_center, 1.0F});
    renderer_config.camera_position = glm::inverse(view_matrix)[3];
    renderer_config.light_ambient_colour = sun.colour;
    renderer_config.light_specular_colour = sun.specular_colour;

    // Step 2: Compute the Light View Matrix
    glm::mat4 light_view_matrix =
        compute_light_view_matrix(transform.position, scene_center);

    // Step 3: Compute Light Space Bounds
    auto &&[light_space_min, light_space_max] =
        compute_light_space_bounds(light_view_matrix, *this);

    auto &shadow_ubo = scene_renderer.get_shadow_configuration();
    const float left = -scene_radius;
    const float right = scene_radius;
    const float bottom = -scene_radius;
    const float top = scene_radius;
    float offset =
        0.5f; // Small positive offset to prevent near clipping of shadows
    float adjusted_near = light_space_min.z + offset;
    const float adjusted_far = 2.0F * scene_radius;

    glm::mat4 light_proj =
        glm::orthoRH_ZO(left, right, bottom, top, adjusted_near, adjusted_far);
    shadow_ubo.projection = light_proj;
    shadow_ubo.view = light_view_matrix;
    shadow_ubo.view_projection = light_proj * light_view_matrix;

    const auto translation =
        glm::translate(glm::mat4(1.0F), transform.position);

    auto frustum_projection =
        glm::perspectiveRH_ZO(glm::radians(90.0F), 1.0F, 2.0F, adjusted_far);

    auto frustum_vulkan = glm::inverse(frustum_projection * light_view_matrix);

    scene_renderer.submit_frustum(frustum_vulkan, translation);
  });

  const auto mesh_view =
      registry.view<const TransformComponent, const MeshComponent>(
          entt::exclude<SunComponent, CameraComponent>);
  // Draw AABBS
  mesh_view.each([&](auto, const auto &transform, const auto &mesh) {
    if (mesh.mesh && mesh.draw_aabb) {
      scene_renderer.submit_aabb(mesh.mesh->get_aabb(), transform.compute());
    }
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

  auto camera_entity = create_entity("Camera");
  camera_entity.add_component<CameraComponent>();
}

auto Scene::on_destroy() -> void {
  for (auto *observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

} // namespace ECS
