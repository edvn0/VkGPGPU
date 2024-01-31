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

auto randomize_span_of_matrices(std::span<Math::Mat4> matrices) -> void {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dis(-1.0F, 1.0F);

  // Randomize span of glm::mat4
  for (auto &mat : matrices) {
    Math::for_each(mat, [&](floating &value) { value = dis(gen); });
  }
}

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), timer(*get_messaging_client()),
      scene_renderer(*get_device()) {
  widgets.emplace_back(make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path()));
};

template <usize N> auto generate_points(floating K) {
  std::array<glm::mat4, N> points{};

  // Random device and generator
  std::random_device rd{};
  std::mt19937_64 gen(rd());

  // Uniform distributions for angle and z-coordinate
  std::uniform_real_distribution<float> angleDistr(0.0f, 2.0f * 3.14159265f);
  std::uniform_real_distribution<float> zDistr(-1.0f, 1.0f);

  for (size_t i = 0; i < N; ++i) {
    // Generate random angle and z-coordinate
    float angle = angleDistr(gen);
    float z = zDistr(gen);

    // Calculate the radius in the XY-plane for this z value
    float xyPlaneRadius = sqrt(1.0f - z * z);

    // Calculate the point position and scale it to a sphere of radius 3
    const auto point =
        glm::vec3(xyPlaneRadius * cos(angle), xyPlaneRadius * sin(angle), z) *
        K;
    points[i] = glm::translate(glm::mat4{1.0F}, point) *
                glm::scale(glm::mat4{1.0F}, glm::vec3(10.0F));
  }
  return points;
}

auto update_camera(auto &camera_position, const auto ts) {
  static constexpr float zoom_speed = 1.0F; // Adjust this value for zoom speed
  static float radius = 17.0F;
  static glm::quat camera_orientation{1.0, 0.0, 0.0, 0.0};
  if (Input::pressed(KeyCode::KEY_D)) {
    glm::quat rotationY = glm::angleAxis(ts * 0.3F, glm::vec3(0, -1, 0));
    camera_orientation = rotationY * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_A)) {
    glm::quat rotationY = glm::angleAxis(-ts * 0.3F, glm::vec3(0, -1, 0));
    camera_orientation = rotationY * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_W)) {
    glm::quat rotationX = glm::angleAxis(ts * 0.3F, glm::vec3(-1, 0, 0));
    camera_orientation = rotationX * camera_orientation;
  }
  if (Input::pressed(KeyCode::KEY_S)) {
    glm::quat rotationX = glm::angleAxis(-ts * 0.3F, glm::vec3(-1, 0, 0));
    camera_orientation = rotationX * camera_orientation;
  }

  if (Input::pressed(KeyCode::KEY_Q)) {
    radius -= zoom_speed * ts;
    if (radius < 1.0F)
      radius = 1.0F;
  }

  if (Input::pressed(KeyCode::KEY_E)) {
    radius += zoom_speed * ts;
  }

  camera_orientation = glm::normalize(camera_orientation);
  const glm::vec3 direction =
      glm::rotate(camera_orientation, glm::vec3(0, 0, -1));
  camera_position = direction * radius;
}

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  update_camera(camera_position, ts);
  scene_renderer.set_frame_index(frame());
  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

  scene->on_update(scene_renderer, ts);

  scene->on_render(scene_renderer, ts, camera_position);
  timer.end();
}

void ClientApp::on_create() {
  scene_renderer.create(*get_swapchain());

  scene = make_scope<ECS::Scene>("Default");
  auto entity = scene->create_entity("Test");

  scene->on_create(*get_device(), *get_window(), *get_swapchain());
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
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
  }

  static constexpr auto draw_stats =
      [](const auto &average,
         const auto &command_buffer_with_compute_stats_available) {
        // Retrieve statistics from the timer and command buffer
        auto &&[frametime, fps] = average.get_statistics();
        auto &&[compute_times] =
            command_buffer_with_compute_stats_available.get_statistics();

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

          // Row for Compute Time
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          UI::text("Compute Time");
          ImGui::TableSetColumnIndex(1);
          UI::text("{} ms", compute_times);

          // End the table
          ImGui::EndTable();
        }
      };

  UI::widget("FPS/Frametime", [&]() {
    draw_stats(get_timer(), scene_renderer.get_command_buffer());
    draw_stats(get_timer(), scene_renderer.get_compute_command_buffer());
  });

  /*UI::widget("Image", [&]() {
    UI::image_drop_button(texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture, {128, 128});
    ImGui::SameLine();
    UI::image(*output_texture_second, {128, 128});
  });*/

  UI::widget("Scene", [&](const auto &extent) {
    UI::image(scene_renderer.get_output_image(), {extent.width, extent.height});
  });

  UI::widget("Depth", [&](const auto &extent) {
    UI::image(scene_renderer.get_depth_image(), {extent.width, extent.height});
  });

  UI::widget("Push constant", [&]() {
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
  scene_renderer.set_extent(new_extent);

  info("{}", new_extent);
}
