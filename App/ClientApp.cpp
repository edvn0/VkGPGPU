#include "pch/vkgpgpu_pch.hpp"

#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Framebuffer.hpp"
#include "Input.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Random.hpp"
#include "Ray.hpp"
#include "SceneWidget.hpp"
#include "UI.hpp"

#include <ImGuizmo.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui-notify/ImGuiNotify.hpp>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include "ecs/Entity.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp"

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), camera(75.0F, get_swapchain()->get_extent().as<float>().width,
                         get_swapchain()->get_extent().as<float>().height, 0.1F,
                         100000.0F, nullptr),
      timer(*get_messaging_client()), scene_renderer(*get_device()){};

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

  camera.on_update(ts);
  scene_renderer.begin_scene(*scene, frame());

  scene->on_update(scene_renderer, ts);

  scene->on_render(scene_renderer, ts, camera.get_projection_matrix(),
                   camera.get_view_matrix());
  timer.end();
}

void ClientApp::on_create() {
  scene_renderer.create(*get_swapchain());

  scene = make_scope<ECS::Scene>("Default");
  auto entity = scene->create_entity("Test");

  scene->on_create(*get_device(), *get_window(), *get_swapchain());

  // ECS::SceneSerialiser serialiser;
  // serialiser.deserialise(*scene, Core::FS::Path{"Default.scene"});

  // scene->temp_on_create(*get_device(), *get_window(), *get_swapchain());

  auto fs_widget = make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path());
  auto widget = make_scope<SceneWidget>(*get_device(), selected_entity);
  widget->set_scene_context(scene.get());

  widgets.emplace_back(std::move(widget));
  widgets.emplace_back(std::move(fs_widget));

  for (const auto &w : widgets) {
    w->on_create(*get_device(), *get_window(), *get_swapchain());
  }

  create_dummy_scene();
}

void ClientApp::on_destroy() {
  Mesh::clear_cache();

  scene_renderer.destroy();

  scene->on_destroy();
  scene.reset();

  for (const auto &widget : widgets) {
    widget->on_destroy();
  }
}

void ClientApp::on_interface(InterfaceSystem &system) {
  // Create a fullscreen window for the dockspace
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  const auto *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

  window_flags &= ~ImGuiWindowFlags_MenuBar;

  // Create the window that will contain the dockspace
  if (ImGui::Begin("DockSpace Demo", nullptr, window_flags)) {
    ImGui::PopStyleVar(3);

    // DockSpace
    const auto dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
  }

  static bool open = true;
  ImGui::ShowDemoWindow(&open);

  static constexpr auto draw_stats =
      [](const auto &average,
         const auto &command_buffer_with_compute_stats_available,
         const auto &other_command_buffer_with_compute_stats_available) {
        // Retrieve statistics from the timer and command buffer
        auto &&[frametime, fps] = average.get_statistics();
        auto &&[compute_times] =
            command_buffer_with_compute_stats_available.get_statistics();
        auto &&[other_compute_times] =
            other_command_buffer_with_compute_stats_available.get_statistics();

        // Start a new ImGui table
        if (ImGui::BeginTable("StatsTable", 2)) {
          // Set up column headers
          ImGui::TableSetupColumn("Statistic");
          ImGui::TableSetupColumn("Value");
          ImGui::TableHeadersRow();

          // Row for frametime
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          UI::text("Frametime");
          ImGui::TableSetColumnIndex(1);
          UI::text("{} ms", frametime);

          // Row for FPS
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          UI::text("FPS");
          ImGui::TableSetColumnIndex(1);
          UI::text("{}", fps);

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          UI::text("Compute command buffer (ms)");
          ImGui::TableSetColumnIndex(1);
          UI::text("{} ms", compute_times);

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          UI::text("Graphics command buffer (ms)");
          ImGui::TableSetColumnIndex(1);
          UI::text("{} ms", other_compute_times);

          // End the table
          ImGui::EndTable();
        }
      };

  UI::widget("FPS/Frametime", [&]() {
    draw_stats(get_timer(), scene_renderer.get_graphics_command_buffer(),
               scene_renderer.get_compute_command_buffer());
  });

  /*UI::widget("Image", [&]() {
    UI::image_drop_button(texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture_second, {128, 128});
  });*/

  UI::widget("Scene", [&](const Extent<float> &extent,
                          const std::tuple<u32, u32> &position) {
    if (!extent.valid())
      return;

    main_position = position;
    main_size = extent;
    UI::image(scene_renderer.get_output_image(), {
                                                     extent.as<u32>().width,
                                                     extent.as<u32>().height,
                                                 });
    camera.set_viewport_size(extent);

    auto &&[x, y] = position;
    viewport_bounds[0] = glm::vec2{
        static_cast<float>(x),
        static_cast<float>(y),
    };
    viewport_bounds[1] = {viewport_bounds[0].x + extent.width,
                          viewport_bounds[0].y + extent.height};

    const auto &view = camera.get_view_matrix();
    const auto &projection = camera.get_projection_matrix();

    const auto payload =
        UI::accept_drag_drop_payload(UI::Identifiers::fs_widget_identifier);
    if (const std::filesystem::path path{payload}; !path.empty()) {
      // Load scene if the file format is .scene
      if (path.extension() == ".scene") {
        scene->clear();
        ECS::SceneSerialiser serialiser;
        serialiser.deserialise(*scene, payload);
        scene->initialise_device_dependent_objects(*get_device());
        scene->sort();
      }

      if (path.extension() == ".gltf" || path.extension() == ".obj" ||
          path.extension() == ".fbx") {
        auto entity = scene->create_entity(path.filename().string());
        entity.add_component<ECS::MeshComponent>(
            Core::Mesh::reference_import_from(*get_device(), path));
      }
    }

    auto entity = load_entity();
    if (!entity)
      return;

    auto &transform_component = entity->get_transform();
    auto transform = transform_component.compute();

    auto y_inverted_projection = projection;
    y_inverted_projection[1][1] *= -1;

    auto &&[width, height] = extent;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width,
                      height);

    bool snap = Input::pressed(KeyCode::KEY_LEFT_CONTROL);
    constexpr float snap_value = 0.5f;
    constexpr std::array snap_modes = {
        snap_value,
        snap_value,
        snap_value,
    };

    if (ImGuizmo::Manipulate(
            Math::value_ptr(view), Math::value_ptr(y_inverted_projection),
            static_cast<ImGuizmo::OPERATION>(this->current_operation),
            ImGuizmo::MODE::LOCAL, Math::value_ptr(transform), nullptr,
            snap ? snap_modes.data() : nullptr)) {

      glm::vec3 scale{};
      glm::quat orientation{};
      glm::vec3 translation{};
      glm::vec3 skew{};
      glm::vec4 perspective{};
      glm::decompose(transform, scale, orientation, translation, skew,
                     perspective);

      switch (current_operation) {
      case GuizmoOperation::T: {
        transform_component.position = translation;
        break;
      }
      case GuizmoOperation::R: {
        transform_component.rotation = orientation;
        break;
      }
      case GuizmoOperation::S: {
        transform_component.scale = scale;
        break;
      }
      default:
        break;
      }
    }
  });

  UI::widget("Depth", [&](const auto &extent) {
    UI::image(scene_renderer.get_depth_image(), {
                                                    extent.as<u32>().width,
                                                    extent.as<u32>().height,
                                                });
  });

  UI::widget("Push constant", [&]() {
    auto &&[grid_colour, plane_colour, grid_size, fog_colour] =
        scene_renderer.get_grid_configuration();
    ImGui::ColorEdit4("Grid Colour", Math::value_ptr(grid_colour));
    ImGui::ColorEdit4("Plane Colour", Math::value_ptr(plane_colour));
    ImGui::SliderFloat2("Grid Size", &grid_size.x, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Near", &grid_size.z, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Far", &grid_size.w, 0.1f, 100.0f);
    ImGui::ColorEdit4("Fog Colour", Math::value_ptr(fog_colour));

    // Assuming bloom is an instance of BloomSettings from the scene_renderer
    auto &bloom = scene_renderer.get_bloom_configuration();

    // UI elements for BloomSettings, updated based on shader details
    ImGui::Checkbox("Bloom Enabled", &bloom.enabled);

    // Since threshold is used in a quadratic threshold function, its sensible
    // range might be small.
    ImGui::SliderFloat("Threshold", &bloom.threshold, 0.0f, 5.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);

    // Knee controls the smoothness of the threshold function, likely needing a
    // tight range.
    ImGui::SliderFloat("Knee", &bloom.knee, 0.0f, 1.0f, "%.3f");

    // Upsample scale may directly relate to texture sampling; a range of 0.5
    // to 2.0 seems reasonable for scaling.
    ImGui::SliderFloat("Upsample Scale", &bloom.upsample_scale, 0.5f, 2.0f,
                       "%.3f");

    // Intensity of the bloom effect, considering shader's potential for
    // significant amplification.
    ImGui::SliderFloat("Intensity", &bloom.intensity, 0.0f, 20.0f, "%.3f");

    // Dirt intensity could be subtle, allowing for a broad range to see its
    // effect clearly.
    ImGui::SliderFloat("Dirt Intensity", &bloom.dirt_intensity, 0.0f, 5.0f,
                       "%.3f");
  });

  UI::widget("Statistics", [&]() {
    const auto &graphics_command_buffer =
        scene_renderer.get_graphics_command_buffer();
    const auto &compute_command_buffer =
        scene_renderer.get_compute_command_buffer();
    const auto &gpu_times = scene_renderer.get_gpu_execution_times();
    ImGui::Text("Dir Shadow Map Pass: %.3fms",
                graphics_command_buffer.get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.directional_shadow_pass_query));
    // ImGui::Text("Spot Shadow Map Pass: %.3fms",
    //             commandBuffer->get_execution_gpu_time(
    //                 scene_renderer.get_current_index(),
    //                 gpu_times.SpotShadowMapPassQuery));
    // ImGui::Text(
    //     "Depth Pre-Pass: %.3fms",
    //     commandBuffer->get_execution_gpu_time(
    //         scene_renderer.get_current_index(),
    //         gpu_times.DepthPrePassQuery));
    /*ImGui::Text("Hierarchical Depth: %.3fms",
                commandBuffer->get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.HierarchicalDepthQuery));
    ImGui::Text(
        "Pre-Integration: %.3fms",
        commandBuffer->get_execution_gpu_time(
            scene_renderer.get_current_index(),
    gpu_times.PreIntegrationQuery));*/
    ImGui::Text("Light Culling Pass: %.3fms",
                compute_command_buffer.get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.light_culling_pass_query));
    ImGui::Text(
        "Geometry Pass: %.3fms",
        graphics_command_buffer.get_execution_gpu_time(
            scene_renderer.get_current_index(), gpu_times.geometry_pass_query));
    /*ImGui::Text(
        "Pre-Convoluted Pass: %.3fms",
        commandBuffer->get_execution_gpu_time(
            scene_renderer.get_current_index(), gpu_times.PreConvolutionQuery));
    ImGui::Text("HBAO Pass: %.3fms", commandBuffer->get_execution_gpu_time(
                                         scene_renderer.get_current_index(),
                                         gpu_times.HBAOPassQuery));
    ImGui::Text("GTAO Pass: %.3fms", commandBuffer->get_execution_gpu_time(
                                         scene_renderer.get_current_index(),
                                         gpu_times.GTAOPassQuery));
    ImGui::Text("GTAO Denoise Pass: %.3fms",
                commandBuffer->get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.GTAODenoisePassQuery));
    ImGui::Text("AO Composite Pass: %.3fms",
                commandBuffer->get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.AOCompositePassQuery));*/
    ImGui::Text("Bloom Pass: %.3fms",
                compute_command_buffer.get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.bloom_compute_pass_query));
    /*ImGui::Text("SSR Pass: %.3fms", commandBuffer->get_execution_gpu_time(
                                        frameIndex, gpu_times.SSRQuery));
    ImGui::Text(
        "SSR Composite Pass: %.3fms",
        commandBuffer->get_execution_gpu_time(
            scene_renderer.get_current_index(), gpu_times.SSRCompositeQuery));
    ImGui::Text(
        "Jump Flood Pass: %.3fms",
        commandBuffer->get_execution_gpu_time(
            scene_renderer.get_current_index(),
    gpu_times.JumpFloodPassQuery));*/
    ImGui::Text("Composite Pass: %.3fms",
                graphics_command_buffer.get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.composite_pass_query));

    const PipelineStatistics &pipelineStats =
        graphics_command_buffer.get_pipeline_statistics(
            scene_renderer.get_current_index());
    ImGui::Text("Input Assembly Vertices: %llu",
                pipelineStats.input_assembly_vertices);
    ImGui::Text("Input Assembly Primitives: %llu",
                pipelineStats.input_assembly_primitives);
    ImGui::Text("Vertex Shader Invocations: %llu",
                pipelineStats.vs_invocations);
    ImGui::Text("Clipping Invocations: %llu", pipelineStats.clip_invocations);
    ImGui::Text("Clipping Primitives: %llu", pipelineStats.clip_primitives);
    ImGui::Text("Fragment Shader Invocations: %llu",
                pipelineStats.fs_invocations);
    ImGui::Text("Compute Shader Invocations: %llu",
                pipelineStats.cs_invocations);
  });

  if (load_entity()) {
    UI::widget(
        "Help", +[]() {
          UI::text("T for Translation");
          UI::text("R for Rotation");
          UI::text("S for Scale");
        });
  }

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  scene_renderer.on_resize(new_extent);

  info("New extent: {}", new_extent);
}

auto ClientApp::delete_selected_entity() -> void {
  auto entity = load_entity();
  if (!entity)
    return;

  // Take a copy of the name
  const auto name = entity->get_name();
  if (scene->delete_entity(entity->get_id())) {
    UI::Toast::success(3000, "Entity '{}' deleted successfully!", name);
  }

  selected_entity.reset();
}

auto ClientApp::load_entity() -> std::optional<ECS::Entity> {
  if (!selected_entity)
    return std::nullopt;

  auto entity = ECS::Entity{scene.get(), *selected_entity};
  if (!entity.valid()) {
    return std::nullopt;
  }
  return entity;
}

static auto cast_ray(const glm::mat4 &projection, const glm::mat4 &view,
                     const glm::vec3 &camera_position, float mx, float my)
    -> std::pair<glm::vec3, glm::vec3> {
  glm::vec4 mouse_clip_pos = {mx, my, -1.0f, 1.0f};

  auto inverse_proj = glm::inverse(projection);
  auto inverse_view = glm::inverse(glm::mat3(view));

  glm::vec4 ray = inverse_proj * mouse_clip_pos;
  glm::vec3 ray_pos = camera_position;
  glm::vec3 ray_dir = inverse_view * glm::vec3(ray);

  return {ray_pos, ray_dir};
}

static auto get_mouse_position_viewport_space(auto x, auto y,
                                              const auto &bounds)
    -> std::pair<float, float> {
  x -= bounds[0].x;
  y -= bounds[0].y;
  auto viewportWidth = bounds[1].x - bounds[0].x;
  auto viewportHeight = bounds[1].y - bounds[0].y;

  return {(x / viewportWidth) * 2.0f - 1.0f,
          ((y / viewportHeight) * 2.0f - 1.0f) * -1.0f};
}

auto ClientApp::pick_object(const glm::vec3 &origin, const glm::vec3 &direction)
    -> entt::entity {
  entt::entity closest = entt::null;
  float closest_value = std::numeric_limits<float>::max();
  auto view = scene->view<ECS::TransformComponent, ECS::MeshComponent>();
  for (auto &&[entity, transform_component, component] : view.each()) {
    if (!component.mesh)
      continue;

    auto &mesh = *component.mesh;
    auto &submeshes = mesh.get_submeshes();
    for (const auto &sm : submeshes) {
      auto &submesh = mesh.get_submesh(sm);
      glm::mat4 transform = transform_component.compute();
      Ray ray = {
          glm::inverse(transform) * glm::vec4(origin, 1.0f),
          glm::inverse(glm::mat3(transform)) * direction,
      };

      float t{};
      bool intersects = ray.intersects_aabb(submesh.bounding_box, t);
      if (intersects) {
        if (t < closest_value) {
          closest_value = t;
          closest = entity;
        }
      }
    }
  }
  return closest;
}

auto ClientApp::copy_selected_entity() -> void {
  if (!selected_entity)
    return;

  auto entity = ECS::Entity{scene.get(), *selected_entity};
  if (!entity.valid())
    return;

  auto copy = scene->create_entity("");

  ECS::SceneSerialiser serialiser{};
  std::stringstream stream{};
  if (!stream)
    return;

  if (!serialiser.serialise_entity_components(stream, entity)) {
    warn("Could not serialise entity to stream");
    return;
  }

  if (!serialiser.deserialise_entity_components(stream, copy)) {
    warn("Could not deserialise entity from stream");
    return;
  }

  static u64 copy_count = 0;
  copy.get_component<ECS::IdentityComponent>().name =
      fmt::format("{} ({})", entity.get_name(), copy_count++);

  scene->initialise_device_dependent_objects(*get_device());
}

void ClientApp::on_event(Event &event) {
  camera.on_event(event);

  EventDispatcher dispatcher(event);
  dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    if (load_entity()) {
      if (event.get_keycode() == KeyCode::KEY_T) {
        current_operation = GuizmoOperation::T;
        return true;
      }
      if (event.get_keycode() == KeyCode::KEY_R) {
        current_operation = GuizmoOperation::R;
        return true;
      }
      if (event.get_keycode() == KeyCode::KEY_S) {
        current_operation = GuizmoOperation::S;
        return true;
      }
    }
    if (event.get_keycode() == KeyCode::KEY_ESCAPE) {
      get_window()->close();
      return true;
    }

    if (event.get_keycode() == KeyCode::KEY_D &&
        Input::pressed(KeyCode::KEY_LEFT_CONTROL)) {
      copy_selected_entity();
      return true;
    }

    if (event.get_keycode() == KeyCode::KEY_K &&
        Input::pressed<KeyCode::KEY_LEFT_ALT>()) {
      scene->clear();
      return true;
    }

    if (event.get_keycode() == KeyCode::KEY_DELETE) {
      delete_selected_entity();
      return true;
    }

    return false;
  });
  dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &event) {
    if (event.get_keycode() == KeyCode::KEY_F11) {
      get_window()->toggle_fullscreen();
      return true;
    }
    return false;
  });
  dispatcher.dispatch<MouseButtonPressedEvent>(
      [this](MouseButtonPressedEvent &event) {
        if (event.get_button() != MouseCode::MOUSE_BUTTON_LEFT)
          return false;

        auto &&[x, y] = event.get_position();

        auto &&[mouse_x, mouse_y] =
            get_mouse_position_viewport_space(x, y, viewport_bounds);

        if (mouse_x < -1.0F || mouse_x > 1.0F || mouse_y < -1.0F ||
            mouse_y > 1.0F)
          return false;

        auto &&[origin, direction] =
            cast_ray(camera.get_projection_matrix(), camera.get_view_matrix(),
                     camera.get_position(), mouse_x, mouse_y);

        auto maybe = pick_object(origin, direction);
        if (maybe != entt::null) {
          selected_entity = maybe;
          return true;
        }

        return false;
      });

  if (event.handled)
    return;

  for (auto &widget : widgets) {
    widget->on_event(event);
    if (event.handled)
      break;
  }
}

void ClientApp::create_dummy_scene() {
  const int grid_size = 10;
  // Assuming max random size is known, add a margin
  const float max_cube_size = 100.0f; // Max size from your random range
  const float margin = 5.0f;          // Additional space between cubes
  const float spacing =
      max_cube_size + margin; // Adjust spacing based on max size and margin

  constexpr glm::vec3 color_start(0.0f, 0.0f, 1.0f); // Blue
  constexpr glm::vec3 color_end(1.0f, 0.65f, 0.0f);  // Orange

  auto little_tokyo = scene->create_entity("Whatever");
  auto &mesh_comp = little_tokyo.add_component<ECS::MeshComponent>();
  mesh_comp.mesh =
      Mesh::reference_import_from(*get_device(), FS::model("sphere.gltf"));

#if 0
  for (int x = 0; x < grid_size; ++x) {
    for (int y = 0; y < grid_size; ++y) {
      for (int z = 0; z < grid_size; ++z) {
        std::string entity_name = "Cube_" + std::to_string(x) + "_" +
                                  std::to_string(y) + "_" + std::to_string(z);
        auto entity = scene->create_entity(entity_name);

        float gradient_factor =
            static_cast<float>(x + y + z) / (grid_size * 3 - 3);
        glm::vec3 gradient_color = color_start * (1.0f - gradient_factor) +
                                   color_end * gradient_factor;

        auto &transform = entity.get_transform();
        transform.position = glm::vec3(x * spacing, y * spacing, z * spacing);

        auto &geom = entity.add_component<ECS::GeometryComponent>();
        float randomSize = Random::as_float(0.5, 100.0F);
        geom.parameters = ECS::BasicGeometry::CubeParameters{randomSize};

        auto randomSize = Random::vec3(0.5, 100.0F);
        entity.get_transform().scale = randomSize;

        entity.add_component<ECS::TextureComponent>(
            glm::vec4{gradient_color, 1.0F});
        entity.add_component<ECS::MeshComponent>(Mesh::reference_import_from(
            *get_device(), FS::model("sphere.gltf")));
      }
    }
  }
#endif

  scene->sort();
}
