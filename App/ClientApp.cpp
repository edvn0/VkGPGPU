#include "pch/vkgpgpu_pch.hpp"

#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Framebuffer.hpp"
#include "Input.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "SceneWidget.hpp"
#include "UI.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include "ecs/Entity.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp"

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), timer(*get_messaging_client()),
      scene_renderer(*get_device()){};

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  camera.update_camera(ts);
  scene_renderer.set_frame_index(frame());
  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

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

  widgets.emplace_back(make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path()));
  auto widget = make_scope<SceneWidget>(*get_device());
  widget->set_scene_context(scene.get());
  widgets.emplace_back(std::move(widget));

  for (const auto &widget : widgets) {
    widget->on_create(*get_device(), *get_window(), *get_swapchain());
  }
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
    draw_stats(get_timer(), scene_renderer.get_command_buffer(),
               scene_renderer.get_compute_command_buffer());
  });

  UI::widget("Selected entity", [&]() {
    if (selected_entity.has_value()) {
      auto &entity = selected_entity.value();
      auto &transform = entity.get_component<ECS::TransformComponent>();

      UI::text("Entity: {}", entity.get_name());
      UI::text("Position: x={}, y={}, z={}", transform.position.x,
               transform.position.y, transform.position.z);
      UI::text("Rotation: x={}, y={}, z={}", transform.rotation.x,
               transform.rotation.y, transform.rotation.z);
      UI::text("Scale: x={}, y={}, z={}", transform.scale.x, transform.scale.y,
               transform.scale.z);
    } else {
      UI::text("No entity selected");
    }
  });

  /*UI::widget("Image", [&]() {
    UI::image_drop_button(texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture_second, {128, 128});
  });*/

  UI::widget("Scene", [&](const Extent<u32> &extent) {
    UI::image(scene_renderer.get_output_image(), {extent.width, extent.height});
    camera.set_aspect_ratio(extent.aspect_ratio());
    if (Input::pressed(MouseCode::MOUSE_BUTTON_LEFT)) {
      float dist_to_nearest = std::numeric_limits<float>::max();
      auto view = scene->get_registry()
                      .view<ECS::TransformComponent, ECS::MeshComponent>(
                          entt::exclude<ECS::CameraComponent>);
      entt::entity nearest_entity = entt::null;
      view.each([&](auto entity, auto &transform, auto &mesh) {
        // Assume meshes are spheres
        const auto &aabb = mesh.mesh->get_aabb();

        glm::vec3 scaledMin = aabb.min() * transform.scale;
        glm::vec3 scaledMax = aabb.max() * transform.scale;

        // Translate the scaled AABB by the entity's position to move it into
        // world space
        glm::vec3 worldMin = scaledMin + transform.position;
        glm::vec3 worldMax = scaledMax + transform.position;
        if (false) {
          nearest_entity = entity;
        }
      });
      if (nearest_entity != entt::null) {
        selected_entity = ECS::Entity{scene.get(), nearest_entity, "Empty"};
      }
      selected_entity = std::nullopt;
    }
  });

  UI::widget("Depth", [&](const auto &extent) {
    UI::image(scene_renderer.get_depth_image(), {extent.width, extent.height});
  });

  UI::widget("Push constant", [&]() {
    auto &camera_position = camera.get_camera_position();
    ImGui::SliderFloat3("Position", &camera_position[0], -10.0f, 10.0f);
    UI::text("Current Position: x={}, y={}, z={}", camera_position.x,
             camera_position.y, camera_position.z);

    auto &sun_position = scene_renderer.get_sun_position();
    ImGui::SliderFloat3("Sun Position", Math::value_ptr(sun_position), -10.0f,
                        10.0f);
    UI::text("Sun Position: x={}, y={}, z={}", sun_position.x, sun_position.y,
             sun_position.z);

    auto &&[value, near, far, bias, depth_value] =
        scene_renderer.get_depth_factors();
    ImGui::SliderFloat("Depth Value", &value, 5.0f, 100.0f);
    ImGui::SliderFloat("Depth Near", &near, -50.F, 50.F);
    ImGui::SliderFloat("Depth Far", &far, 0.1f, 100.0f);
    ImGui::SliderFloat("Depth Bias", &bias, 0.0f, 0.1F);
    ImGui::SliderFloat("Depth Factor", &depth_value, 0.01f, 1.0f);

    auto &&[grid_colour, plane_colour, grid_size, fog_colour] =
        scene_renderer.get_grid_configuration();
    ImGui::ColorEdit4("Grid Colour", Math::value_ptr(grid_colour));
    ImGui::ColorEdit4("Plane Colour", Math::value_ptr(plane_colour));
    ImGui::SliderFloat2("Grid Size", &grid_size.x, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Near", &grid_size.z, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Far", &grid_size.w, 0.1f, 100.0f);
    ImGui::ColorEdit4("Fog Colour", Math::value_ptr(fog_colour));
  });

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  scene_renderer.on_resize(new_extent);

  info("{}", new_extent);
}
