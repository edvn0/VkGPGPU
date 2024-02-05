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

#include <ImGuizmo.h>
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
    : App(props), timer(*get_messaging_client()), scene_renderer(*get_device()),
      camera(75.0F, get_swapchain()->get_extent().as<float>().width,
             get_swapchain()->get_extent().as<float>().height, 0.1F, 1000.0F,
             nullptr){};

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  camera.on_update(ts);
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

  auto fs_widget = make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path());
  auto widget = make_scope<SceneWidget>(*get_device(), selected_entity);
  widget->set_scene_context(scene.get());

  widgets.emplace_back(std::move(widget));
  widgets.emplace_back(std::move(fs_widget));

  for (const auto &w : widgets) {
    w->on_create(*get_device(), *get_window(), *get_swapchain());
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

  UI::widget("Scene", [&](const Extent<u32> &extent) {
    UI::image(scene_renderer.get_output_image(), {extent.width, extent.height});
    camera.set_viewport_size(extent);

    const auto &view = camera.get_view_matrix();
    const auto &projection = camera.get_projection_matrix();

    if (!selected_entity)
      return;

    auto entity = scene->get_entity(*selected_entity);
    auto &transform_component = entity.get_transform();
    auto transform = transform_component.compute();

    auto y_inverted_projection = projection;
    y_inverted_projection[1][1] *= -1;

    auto [width, height] = extent.as<float>();

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width,
                      height);

    bool snap = Input::pressed(KeyCode::KEY_LEFT_CONTROL);
    constexpr float snap_value = 0.5f;
    constexpr std::array snap_modes = {snap_value, snap_value, snap_value};

    if (ImGuizmo::Manipulate(
            Math::value_ptr(view), Math::value_ptr(y_inverted_projection),
            static_cast<ImGuizmo::OPERATION>(this->current_operation),
            ImGuizmo::MODE::LOCAL, Math::value_ptr(transform), nullptr,
            snap ? snap_modes.data() : nullptr)) {

      glm::vec3 scale;
      glm::quat orientation;
      glm::vec3 translation;
      glm::vec3 skew;
      glm::vec4 perspective;
      glm::decompose(transform, scale, orientation, translation, skew,
                     perspective);

      // Perform smooth updates
      switch (current_operation) {
      case GuizmoOperation::T: {
        transform_component.position = translation;
        break;
      }
      case GuizmoOperation::R: {
        // Do this in Euler in an attempt to preserve any full revolutions (>
        // 360)
        glm::vec3 original_rotation_euler_angles =
            transform_component.get_rotation_in_euler_angles();

        // Map original rotation to range [-180, 180] which is what ImGuizmo
        // gives us
        original_rotation_euler_angles.x =
            fmodf(original_rotation_euler_angles.x + glm::pi<float>(),
                  glm::two_pi<float>()) -
            glm::pi<float>();
        original_rotation_euler_angles.y =
            fmodf(original_rotation_euler_angles.y + glm::pi<float>(),
                  glm::two_pi<float>()) -
            glm::pi<float>();
        original_rotation_euler_angles.z =
            fmodf(original_rotation_euler_angles.z + glm::pi<float>(),
                  glm::two_pi<float>()) -
            glm::pi<float>();

        glm::vec3 deltaRotationEuler =
            glm::eulerAngles(orientation) - original_rotation_euler_angles;

        // Try to avoid drift due numeric precision
        if (fabs(deltaRotationEuler.x) < 0.001)
          deltaRotationEuler.x = 0.0f;
        if (fabs(deltaRotationEuler.y) < 0.001)
          deltaRotationEuler.y = 0.0f;
        if (fabs(deltaRotationEuler.z) < 0.001)
          deltaRotationEuler.z = 0.0f;

        transform_component.set_rotation_as_euler_angles(
            transform_component.get_rotation_in_euler_angles() +=
            deltaRotationEuler);
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
    UI::image(scene_renderer.get_depth_image(), {extent.width, extent.height});
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
  });

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  scene_renderer.on_resize(new_extent);

  info("{}", new_extent);
}

void ClientApp::on_event(Event &event) {
  camera.on_event(event);

  EventDispatcher dispatcher(event);
  dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    if (event.get_keycode() == KeyCode::KEY_T) {
      current_operation = ClientApp::cycle(current_operation);
      return true;
    }
    if (event.get_keycode() == KeyCode::KEY_ESCAPE) {
      get_window()->close();
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

  if (event.handled)
    return;

  for (auto &widget : widgets) {
    widget->on_event(event);
    if (event.handled)
      break;
  }
}

auto ClientApp::cycle(GuizmoOperation current) -> GuizmoOperation {
  switch (current) {
  case GuizmoOperation::T:
    return GuizmoOperation::R; // T -> R
  case GuizmoOperation::R:
    return GuizmoOperation::S; // R -> S
  case GuizmoOperation::S:
    return GuizmoOperation::T; // S -> T
  default:
    return GuizmoOperation::T; // Default to T if not currently T, R, or S
  }
}
