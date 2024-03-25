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
    auto aabb = BasicGeometry::get_aabb_for_geometry(geometry.parameters,
                                                     transform.compute());
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
      glm::vec3(0, 1, 0)); // Assuming Y-up in your coordinate system
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

auto Scene::save(std::string_view path) -> bool {
  Core::FS::Path chosen = path.empty()
                              ? name + ".scene"
                              : std::string{path} + "-" + name + ".scene";
  if (path.empty()) {
    auto maybe_full_path =
        Core::UI::save_file_dialog(Core::FS::scenes_directory());

    if (!maybe_full_path) {
      Core::UI::Toast::error(3000, "No path chosen in the dialog.");
      return false;
    }

    chosen = maybe_full_path.value();

    if (!Core::FS::exists(chosen)) {
      Core::UI::Toast::error(
          3000, "The path chosen in the dialog did not exist. Terrible error!");
      return false;
    }
  }

  SceneSerialiser serialiser;
  try {
    serialiser.serialise(*this, Core::FS::Path{chosen});
    Core::UI::Toast::success(3000, "Saved the scene to: '{}'", chosen);
    return true;
  } catch (const std::exception &e) {
    Core::UI::Toast::error(3000, "Exception: {}", e);
    return false;
  }
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

auto Scene::on_render_runtime(Core::SceneRenderer &renderer, Core::floating dt)
    -> void {
  auto camera = registry.view<const CameraComponent>();
  if (camera.size() == 0)
    return;

  {
    auto point_light_view =
        registry.group<PointLightComponent>(entt::get<TransformComponent>);
    light_environment.point_light_buffer.resize(point_light_view.size());
    Core::u32 point_light_index = 0;
    for (auto e : point_light_view) {
      Entity entity(this, e);
      auto [transform_component, light_component] =
          point_light_view.get<TransformComponent, PointLightComponent>(e);
      light_environment.point_light_buffer[point_light_index++] = {
          transform_component.position, light_component.intensity,
          light_component.radiance,     light_component.min_radius,
          light_component.radius,       light_component.falloff,
          light_component.light_size,   light_component.casts_shadows,
      };
    }
  }
  // Spot Lights
  {
    auto spot_light_view =
        registry.group<SpotLightComponent>(entt::get<TransformComponent>);
    light_environment.spot_light_buffer.resize(spot_light_view.size());
    Core::u32 spot_light_index = 0;
    for (auto e : spot_light_view) {
      Entity entity(this, e);
      auto [transform_component, light_component] =
          spot_light_view.get<TransformComponent, SpotLightComponent>(e);
      glm::vec3 direction = glm::normalize(glm::rotate(
          transform_component.rotation, glm::vec3(1.0f, 0.0f, 0.0f)));

      light_environment.spot_light_buffer[spot_light_index++] = {
          transform_component.position,
          light_component.intensity,
          direction,
          light_component.angle_attenuation,
          light_component.radiance,
          light_component.range,
          light_component.angle,
          light_component.falloff,
          light_component.soft_shadows,
          {},
          light_component.casts_shadows,
      };
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

  const auto &cam = registry.get<CameraComponent>(camera.front());
  const auto &view_matrix = glm::mat4{1.0F}; // cam.get_view_matrix();
  const auto &projection_matrix =
      glm::mat4{1.0F}; // cam.get_projection_matrix();

  registry.view<TransformComponent, CameraComponent>().each(
      [&](auto, auto &transform, auto &) {
        transform.position = glm::inverse(view_matrix)[3];
      });

  const auto sun_view =
      registry.view<const SunComponent, const TransformComponent>();
  sun_view.each([&](auto, const SunComponent &sun, const auto &transform) {
    // Update the scene renderer with the sun's position and depth factors
    renderer.get_sun_position() = transform.position;
    renderer.get_depth_factors() = sun.depth_params;

    // Update renderer configuration
    auto &renderer_config = renderer.get_renderer_configuration();
    renderer_config.view = view_matrix;
    renderer_config.projection = projection_matrix;
    renderer_config.view_projection = projection_matrix * view_matrix;
    renderer_config.inverse_view = glm::inverse(view_matrix);
    renderer_config.inverse_projection = glm::inverse(projection_matrix);
    renderer_config.inverse_view_projection =
        renderer_config.inverse_view * renderer_config.inverse_projection;
    renderer_config.light_position = glm::vec4{transform.position, 1.0F};

    // Update light direction based on the scene center
    renderer_config.light_direction =
        -glm::normalize(renderer_config.light_position -
                        glm::vec4{sun.depth_params.center, 1.0F});
    renderer_config.camera_position = glm::inverse(view_matrix)[3];
    renderer_config.light_ambient_colour = sun.colour;
    renderer_config.light_specular_colour = sun.specular_colour;

    float depth_linearized_mul =
        (-projection_matrix[3][2]); // float depth_linearized_mul = ( clipFar *
                                    // clipNear ) / ( clipFar - clipNear );
    float depth_linearized_add =
        (projection_matrix[2][2]); // float depth_linearized_add = clipFar / (
                                   // clipFar - clipNear );
    // correct the handedness issue.
    if (depth_linearized_mul * depth_linearized_add < 0)
      depth_linearized_add = -depth_linearized_add;
    renderer_config.depth_unpacked_constants = {
        depth_linearized_mul,
        depth_linearized_add,
    };
    const auto *P = glm::value_ptr(projection_matrix);
    const glm::vec4 projection_info_perspective = {
        2.0f / (P[4 * 0 + 0]),                 // (x) * (R - L)/N
        2.0f / (P[4 * 1 + 1]),                 // (y) * (T - B)/N
        -(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0], // L/N
        -(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1], // B/N
    };
    float tan_half_fov_y =
        1.0f /
        renderer_config.projection[1][1]; // = tanf( drawContext.Camera.GetYFOV(
                                          // ) * 0.5f );
    float tan_half_fov_x =
        1.0f /
        renderer_config.projection[0][0]; // = tan_half_fov_y *
                                          // drawContext.Camera.GetAspect( );
    renderer_config.camera_tan_half_fov = {
        tan_half_fov_x,
        tan_half_fov_y,
    };
    renderer_config.ndc_to_view_multiplied = {
        projection_info_perspective[0],
        projection_info_perspective[1],
    };
    renderer_config.ndc_to_view_added = {
        projection_info_perspective[2],
        projection_info_perspective[3],
    };

    // Step 2: Compute the Light View Matrix
    glm::mat4 light_view_matrix =
        compute_light_view_matrix(transform.position, sun.depth_params.center);
  glm_:
    auto &shadow_ubo = renderer.get_shadow_configuration();
    glm::mat4 light_proj =
        glm::ortho(sun.depth_params.lrbt.x, sun.depth_params.lrbt.y,
                   sun.depth_params.lrbt.z, sun.depth_params.lrbt.w,
                   sun.depth_params.nf.x, sun.depth_params.nf.y);
    shadow_ubo.projection = light_proj;
    shadow_ubo.view = light_view_matrix;
    shadow_ubo.view_projection = light_proj * light_view_matrix;
  });

  const auto mesh_view =
      registry.view<const TransformComponent, const MeshComponent>(
          entt::exclude<SunComponent, CameraComponent>);
  // Draw AABBS
  mesh_view.each([&](auto, const auto &transform, const auto &mesh) {
    if (mesh.mesh && mesh.draw_aabb) {
      renderer.submit_aabb(mesh.mesh->get_aabb(), transform.compute());
    }
  });

  renderer.begin_frame(projection_matrix, view_matrix);
  renderer.flush();
  renderer.end_frame();
}

auto Scene::on_render_editor(Core::SceneRenderer &renderer, Core::floating dt,
                             const Core::EditorCamera &editor_camera) -> void {
  {
    auto point_light_view =
        registry.group<PointLightComponent>(entt::get<TransformComponent>);
    light_environment.point_light_buffer.resize(point_light_view.size());
    Core::u32 point_light_index = 0;
    for (auto e : point_light_view) {
      Entity entity(this, e);
      auto [transform_component, light_component] =
          point_light_view.get<TransformComponent, PointLightComponent>(e);
      light_environment.point_light_buffer[point_light_index++] = {
          transform_component.position, light_component.intensity,
          light_component.radiance,     light_component.min_radius,
          light_component.radius,       light_component.falloff,
          light_component.light_size,   light_component.casts_shadows,
      };
    }
  }
  // Spot Lights
  {
    auto spot_light_view =
        registry.group<SpotLightComponent>(entt::get<TransformComponent>);
    light_environment.spot_light_buffer.resize(spot_light_view.size());
    Core::u32 spot_light_index = 0;
    for (auto e : spot_light_view) {
      Entity entity(this, e);
      auto [transform_component, light_component] =
          spot_light_view.get<TransformComponent, SpotLightComponent>(e);
      glm::vec3 direction = glm::normalize(glm::rotate(
          transform_component.rotation, glm::vec3(1.0f, 0.0f, 0.0f)));

      light_environment.spot_light_buffer[spot_light_index++] = {
          transform_component.position,
          light_component.intensity,
          direction,
          light_component.angle_attenuation,
          light_component.radiance,
          light_component.range,
          light_component.angle,
          light_component.falloff,
          light_component.soft_shadows,
          {},
          light_component.casts_shadows,
      };
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
            renderer.submit_aabb(
                BasicGeometry::get_aabb_for_geometry(arg, computed));
          }
        },
        geom.parameters);
  });

  const auto &view_matrix = editor_camera.get_view_matrix();
  const auto &projection_matrix = editor_camera.get_projection_matrix();

  registry.view<TransformComponent, CameraComponent>().each(
      [&](auto, auto &transform, auto &) {
        transform.position = glm::inverse(view_matrix)[3];
      });

  const auto sun_view =
      registry.view<const SunComponent, const TransformComponent>();
  sun_view.each([&](auto, const SunComponent &sun, const auto &transform) {
    // Update the scene renderer with the sun's position and depth factors
    renderer.get_sun_position() = transform.position;
    renderer.get_depth_factors() = sun.depth_params;

    // Update renderer configuration
    auto &renderer_config = renderer.get_renderer_configuration();
    renderer_config.view = view_matrix;
    renderer_config.projection = projection_matrix;
    renderer_config.view_projection = projection_matrix * view_matrix;
    renderer_config.inverse_view = glm::inverse(view_matrix);
    renderer_config.inverse_projection = glm::inverse(projection_matrix);
    renderer_config.inverse_view_projection =
        renderer_config.inverse_view * renderer_config.inverse_projection;
    renderer_config.light_position = glm::vec4{transform.position, 1.0F};

    // Update light direction based on the scene center
    renderer_config.light_direction =
        -glm::normalize(renderer_config.light_position -
                        glm::vec4{sun.depth_params.center, 1.0F});
    renderer_config.camera_position = glm::inverse(view_matrix)[3];
    renderer_config.light_ambient_colour = sun.colour;
    renderer_config.light_specular_colour = sun.specular_colour;

    float depth_linearized_mul =
        (-projection_matrix[3][2]); // float depth_linearized_mul = ( clipFar *
                                    // clipNear ) / ( clipFar - clipNear );
    float depth_linearized_add =
        (projection_matrix[2][2]); // float depth_linearized_add = clipFar / (
                                   // clipFar - clipNear );
    // correct the handedness issue.
    if (depth_linearized_mul * depth_linearized_add < 0)
      depth_linearized_add = -depth_linearized_add;
    renderer_config.depth_unpacked_constants = {
        depth_linearized_mul,
        depth_linearized_add,
    };
    const auto *P = glm::value_ptr(projection_matrix);
    const glm::vec4 projection_info_perspective = {
        2.0f / (P[4 * 0 + 0]),                 // (x) * (R - L)/N
        2.0f / (P[4 * 1 + 1]),                 // (y) * (T - B)/N
        -(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0], // L/N
        -(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1], // B/N
    };
    float tan_half_fov_y =
        1.0f /
        renderer_config.projection[1][1]; // = tanf( drawContext.Camera.GetYFOV(
                                          // ) * 0.5f );
    float tan_half_fov_x =
        1.0f /
        renderer_config.projection[0][0]; // = tan_half_fov_y *
                                          // drawContext.Camera.GetAspect( );
    renderer_config.camera_tan_half_fov = {
        tan_half_fov_x,
        tan_half_fov_y,
    };
    renderer_config.ndc_to_view_multiplied = {
        projection_info_perspective[0],
        projection_info_perspective[1],
    };
    renderer_config.ndc_to_view_added = {
        projection_info_perspective[2],
        projection_info_perspective[3],
    };

    // Step 2: Compute the Light View Matrix
    glm::mat4 light_view_matrix =
        compute_light_view_matrix(transform.position, sun.depth_params.center);

    auto &shadow_ubo = renderer.get_shadow_configuration();
    glm::mat4 light_proj =
        glm::ortho(sun.depth_params.lrbt.x, sun.depth_params.lrbt.y,
                   sun.depth_params.lrbt.z, sun.depth_params.lrbt.w,
                   sun.depth_params.nf.x, sun.depth_params.nf.y);
    shadow_ubo.projection = light_proj;
    shadow_ubo.view = light_view_matrix;
    shadow_ubo.view_projection = light_proj * light_view_matrix;
  });

  const auto mesh_view =
      registry.view<const TransformComponent, const MeshComponent>(
          entt::exclude<SunComponent, CameraComponent>);
  // Draw AABBS
  mesh_view.each([&](auto, const auto &transform, const auto &mesh) {
    if (mesh.mesh && mesh.draw_aabb) {
      renderer.submit_aabb(mesh.mesh->get_aabb(), transform.compute());
    }
  });

  renderer.begin_frame(projection_matrix, view_matrix);
  renderer.flush();
  renderer.end_frame();
}

auto Scene::on_render_simulation(Core::SceneRenderer &renderer,
                                 Core::floating dt,
                                 const Core::EditorCamera &editor_camera)
    -> void {
  on_render_editor(renderer, dt, editor_camera);
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

auto Scene::on_update_runtime(Core::floating ts) -> void {

  if (should_simulate) {
    // Physics, audio etc
  }
}

auto Scene::on_runtime_start() -> void {
  is_playing = true;
  should_simulate = false;
}

auto Scene::on_runtime_stop() -> void {
  is_playing = false;
  should_simulate = false;
}

auto Scene::on_simulation_start() -> void {
  is_playing = true;
  should_simulate = true;
}
auto Scene::on_simulation_stop() -> void {
  is_playing = false;
  should_simulate = false;
}

auto Scene::on_update_editor(Core::floating ts) -> void {
  while (!futures.empty()) {
    auto &future = futures.front();
    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      future.get();
      futures.pop();
    } else {
      break;
    }
  }
}

auto Scene::copy_to(Scene &other) -> void {

  static SceneSerialiser serialiser;
  std::stringstream output;
  if (!output)
    return;
  serialiser.serialise(*this, output);

  serialiser.deserialise(other, output);
}

} // namespace ECS
