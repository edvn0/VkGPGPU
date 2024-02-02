#include "pch/vkgpgpu_pch.hpp"

#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Framebuffer.hpp"
#include "Input.hpp"
#include "Material.hpp"
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
      scene_renderer(*get_device()) {
  widgets.emplace_back(make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path()));
};

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

  /*UI::widget("Image", [&]() {
    UI::image_drop_button(texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture_second, {128, 128});
  });*/

  UI::widget("Scene", [&](const auto &extent) {
    if (Input::pressed(MouseCode::MOUSE_BUTTON_LEFT)) {

      const auto mouse_position = Input::mouse_position();

      // Get the widget's screen position and size
      const ImVec2 widget_pos = ImGui::GetCursorScreenPos();
      ImVec2 widget_size =
          ImGui::GetContentRegionAvail(); // or another method to get the size

      // Calculate mouse position relative to the widget
      ImVec2 mouse_pos_relative_to_widget;
      mouse_pos_relative_to_widget.x = mouse_position.x - widget_pos.x;
      mouse_pos_relative_to_widget.y = mouse_position.y - widget_pos.y;

      glm::vec2 ndc;
      ndc.x = (2.0f * mouse_pos_relative_to_widget.x) / widget_size.x - 1.0f;
      ndc.y = 1.0f - (2.0f * mouse_pos_relative_to_widget.y) / widget_size.y;

      ndc.x *= extent.aspect_ratio();

      const auto ray_clip = glm::vec4{ndc.x, ndc.y, -1.0F, 1.0F};
      auto ray_eye = glm::inverse(camera.get_projection_matrix()) * ray_clip;
      ray_eye = glm::vec4{ray_eye.x, ray_eye.y, -1.0F, 0.0F};

      auto ray_world = glm::inverse(camera.get_view_matrix()) * ray_eye;
      ray_world = glm::normalize(
          glm::vec4{ray_world.x, ray_world.y, ray_world.z, 0.0F});

      const auto ray_origin = camera.get_camera_position();
    }

    UI::image(scene_renderer.get_output_image(), {extent.width, extent.height});
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
  });

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  scene_renderer.on_resize(new_extent);

  info("{}", new_extent);
}
