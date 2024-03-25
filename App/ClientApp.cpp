#include "pch/vkgpgpu_pch.hpp"

#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "FilesystemWidget.hpp"
#include "Formatters.hpp"
#include "Framebuffer.hpp"
#include "Input.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Random.hpp"
#include "Ray.hpp"
#include "SceneWidget.hpp"
#include "UI.hpp"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui-notify/ImGuiNotify.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <vulkan/vulkan_core.h>

#include "ecs/Entity.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp"

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), camera(75.0F, get_swapchain()->get_extent().as<float>().width,
                         get_swapchain()->get_extent().as<float>().height, 0.1F,
                         1000.0F, nullptr),
      timer(*get_messaging_client()), scene_renderer(*get_device()){};

void ClientApp::on_update(floating ts) {
  timer.begin();

  for (const auto &widget : widgets) {
    widget->on_update(ts);
  }

  switch (scene_state_fsm.get_current_state()) {
  case SceneState::Edit: {
    camera.set_active(camera_can_receive_events);
    camera.on_update(ts);
    scene_renderer.begin_scene(*active_scene, frame());
    active_scene->on_update_editor(ts);
    active_scene->on_render_editor(scene_renderer, ts, camera);
    break;
  }
  case SceneState::Play: {
    scene_renderer.begin_scene(*runtime_scene, frame());
    runtime_scene->on_update_runtime(ts);

    if (editor_camera_in_runtime) {
      camera.set_active(viewport_mouse_hovered || camera_can_receive_events);
      camera.on_update(ts);
      runtime_scene->on_render_editor(scene_renderer, ts, camera);
#if 0
      on_render_2d();
#endif
    } else {
      runtime_scene->on_render_runtime(scene_renderer, ts);
    }
    break;
  }
  case SceneState::Simulate: {
    scene_renderer.begin_scene(*simulation_scene, frame());
    camera.set_active(camera_can_receive_events);
    camera.on_update(ts);
    simulation_scene->on_update_runtime(ts);
    simulation_scene->on_render_simulation(scene_renderer, ts, camera);
    break;
  }
  case SceneState::Pause: {
    scene_renderer.begin_scene(*runtime_scene, frame());
    camera.set_active(viewport_mouse_hovered);
    camera.on_update(ts);
    runtime_scene->on_render_runtime(scene_renderer, ts);
    break;
  }
  }

  if (Input::pressed(MouseCode::MOUSE_BUTTON_RIGHT) &&
      !right_click_started_in_viewport && viewport_focused &&
      viewport_mouse_hovered)
    right_click_started_in_viewport = true;

  if (!Input::pressed(MouseCode::MOUSE_BUTTON_RIGHT))
    right_click_started_in_viewport = false;

  // AssetEditorPanel::on_update(ts);

  // SceneRenderer::WaitForThreads();

  // if (ScriptEngine::ShouldReloadAppAssembly())
  //   ScriptEngine::ReloadAppAssembly();
  timer.end();
}

class Watcher : public IFilesystemChangeListener {
public:
  Watcher(const Device &dev, ShaderCache &shader_cache)
      : device(&dev), cache(shader_cache) {}
  ~Watcher() override = default;

  auto get_file_extension_filter()
      -> const Container::StringLikeUnorderedSet<std::string> & override {
    return filetype_extensions;
  }

  auto on_file_created(const FileInfo &info) -> IterationDecision override {
    return handle(info.path);
  };

  auto on_file_modified(const FileInfo &info) -> IterationDecision override {
    return handle(info.path);
  };

private:
  auto handle(const FS::Path &path) -> IterationDecision {
    const auto maybe_type = determine_shader_type(path);
    if (!maybe_type)
      return IterationDecision::Continue;

    try {
      std::scoped_lock lock{mutex};
      auto &shader =
          get_or_create(path.filename().replace_extension().string());

      if (*maybe_type == Shader::Type::Vertex) {
        auto fragment_path =
            FS::resolve(FS::shader_directory() /
                        path.filename().replace_extension(".frag"));
        if (!FS::exists(fragment_path)) {
          error("Could not find fragment shader '{}' associated with this "
                "vertex shader '{}'",
                path, fragment_path);
          return IterationDecision::Continue;
        }
        auto compiled =
            Shader::compile_graphics_scoped(*device, path, fragment_path);
        if (!compiled)
          return IterationDecision::Continue;
        shader.swap(compiled);
      } else if (*maybe_type == Shader::Type::Fragment) {
        auto vertex_path =
            FS::resolve(FS::shader_directory() /
                        path.filename().replace_extension(".vert"));
        if (!FS::exists(vertex_path)) {
          error("Could not find vertex shader '{}' associated with this "
                "fragment shader '{}'",
                vertex_path, path);
          return IterationDecision::Continue;
        }
        auto compiled =
            Shader::compile_graphics_scoped(*device, vertex_path, path);
        if (!compiled)
          return IterationDecision::Continue;
        shader.swap(compiled);
      } else if (*maybe_type == Shader::Type::Compute) {
        auto compiled = Shader::compile_compute_scoped(*device, path);
        if (!compiled)
          return IterationDecision::Continue;
        shader.swap(compiled);
      }
    } catch (const std::exception &) {
      return IterationDecision::Continue;
    }
    return IterationDecision::Break;
  }

  auto determine_shader_type(const FS::Path &path)
      -> std::optional<Shader::Type> {
    if (path.extension().string() == ".vert")
      return Shader::Type::Vertex;
    if (path.extension().string() == ".frag")
      return Shader::Type::Fragment;
    if (path.extension().string() == ".comp")
      return Shader::Type::Compute;
    return {};
  };

  auto get_or_create(const std::string &filename) -> Scope<Shader> & {
    if (cache.contains(filename))
      return cache.at(filename);
    cache[filename] = nullptr;
    return cache.at(filename);
  }

  Container::StringLikeUnorderedSet<std::string> filetype_extensions{
      ".glsl", ".vert", ".frag", ".comp"};

  std::mutex mutex;
  const Device *device;
  ShaderCache &cache;
};

void ClientApp::on_create() {
  scene_renderer.create(*get_swapchain());

  editor_scene = make_ref<ECS::Scene>("Default");
  active_scene = editor_scene;
  active_scene->on_create(*get_device(), *get_window(), *get_swapchain());

  this->file_system_hook(
      make_scope<Watcher>(*get_device(), scene_renderer.get_shader_cache()));

  // ECS::SceneSerialiser serialiser;
  // serialiser.deserialise(*scene, Core::FS::Path{"Default.scene"});

  // active_scene->temp_on_create(*get_device(), *get_window(),
  // *get_swapchain());

  auto fs_widget = make_scope<FilesystemWidget>(
      *get_device(), std::filesystem::current_path());
  auto widget = make_scope<SceneWidget>(*get_device(), selected_entity);
  widget->set_scene_context(active_scene.get());

  scene_context_dependents.push_back(widget.get());

  widgets.emplace_back(std::move(widget));
  widgets.emplace_back(std::move(fs_widget));

  for (const auto &w : widgets) {
    w->on_create(*get_device(), *get_window(), *get_swapchain());
  }

  create_dummy_scene();

  play_icon = Texture::construct_shader(
      *get_device(), {
                         .format = ImageFormat::UNORM_RGBA8,
                         .path = FS::editor_resources("Play.png"),
                         .mip_generation = MipGeneration(1),
                     });
  pause_icon = Texture::construct_shader(
      *get_device(), {
                         .format = ImageFormat::UNORM_RGBA8,
                         .path = FS::editor_resources("Pause.png"),
                         .mip_generation = MipGeneration(1),
                     });
  simulate_icon = Texture::construct_shader(
      *get_device(), {
                         .format = ImageFormat::UNORM_RGBA8,
                         .path = FS::editor_resources("Simulate.png"),
                         .mip_generation = MipGeneration(1),
                     });
  stop_icon = Texture::construct_shader(
      *get_device(), {
                         .format = ImageFormat::UNORM_RGBA8,
                         .path = FS::editor_resources("Stop.png"),
                         .mip_generation = MipGeneration(1),
                     });
}

void ClientApp::on_destroy() {
  Mesh::clear_cache();

  scene_renderer.destroy();

  active_scene->on_destroy();
  if (editor_scene) {
    editor_scene->on_destroy();
  }
  if (simulation_scene) {
    simulation_scene->on_destroy();
  }
  if (runtime_scene) {
    runtime_scene->on_destroy();
  }
  active_scene.reset();
  editor_scene.reset();
  simulation_scene.reset();
  runtime_scene.reset();

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

  auto window_pos = get_window()->get_position();
  UI::widget("Scene", [&](const Extent<float> &extent,
                          const std::tuple<u32, u32> &position) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    viewport_mouse_hovered = ImGui::IsWindowHovered();
    viewport_focused = ImGui::IsWindowFocused();

    if (!extent.valid())
      return;

    main_position = position;
    main_size = extent;
    UI::image(scene_renderer.get_output_image(), {
                                                     extent.as<u32>().width,
                                                     extent.as<u32>().height,
                                                 });
    camera.set_viewport_size(extent);

    // These are relative to bottom right
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    vMin.x += ImGui::GetWindowPos().x;
    vMin.y += ImGui::GetWindowPos().y;
    vMax.x += ImGui::GetWindowPos().x;
    vMax.y += ImGui::GetWindowPos().y;

    auto &&[window_pos_x, window_pos_y] = window_pos;
    viewport_bounds[0] = {
        vMin.x - window_pos_x,
        vMin.y - window_pos_y,
    };
    viewport_bounds[1] = {
        vMax.x - window_pos_x,
        vMax.y - window_pos_y,
    };
    const auto is_hovering = ImGui::IsWindowHovered();

    camera_can_receive_events =
        (is_hovering && viewport_focused) || right_click_started_in_viewport;

    const auto &view = camera.get_view_matrix();
    const auto &projection = camera.get_projection_matrix();

    const auto payload =
        UI::accept_drag_drop_payload(UI::Identifiers::fs_widget_identifier);
    if (const std::filesystem::path path{payload}; !path.empty()) {
      // Load scene if the file format is .scene
      if (path.extension() == ".scene") {
        active_scene->clear();
        ECS::SceneSerialiser serialiser;
        serialiser.deserialise(*active_scene, payload);
        active_scene->initialise_device_dependent_objects(*get_device());
        active_scene->sort();
      }

      if (path.extension() == ".gltf" || path.extension() == ".obj" ||
          path.extension() == ".fbx" || path.extension() == ".glb") {
        auto entity = active_scene->create_entity(path.filename().string());
        entity.add_component<ECS::MeshComponent>(
            Core::Mesh::reference_import_from(*get_device(), path));
      }
    }

    auto entity = load_entity();
    if (!entity) {
      ImGui::PopStyleVar(3);
      return;
    }

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

    ImGui::PopStyleVar(3);
  });

  UI::widget("Depth", [&](const Extent<float> &extent) {
    UI::image(scene_renderer.get_depth_image(), {
                                                    .extent = extent.as<u32>(),
                                                    .flipped = true,
                                                });
  });

  UI::widget("Renderer settings", [&]() {
    auto &&[grid_colour, plane_colour, grid_size, fog_colour] =
        scene_renderer.get_grid_configuration();
    ImGui::ColorEdit4("Grid Colour", Math::value_ptr(grid_colour));
    ImGui::ColorEdit4("Plane Colour", Math::value_ptr(plane_colour));
    ImGui::SliderFloat2("Grid Size", &grid_size.x, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Near", &grid_size.z, 0.1f, 100.0f);
    ImGui::SliderFloat("Grid Far", &grid_size.w, 0.1f, 100.0f);
    ImGui::ColorEdit4("Fog Colour", Math::value_ptr(fog_colour));

    auto &bloom = scene_renderer.get_bloom_configuration();

    ImGui::Checkbox("Bloom Enabled", &bloom.enabled);
    ImGui::SliderFloat("Threshold", &bloom.threshold, 0.0f, 5.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Knee", &bloom.knee, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Opacity", &bloom.opacity, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Upsample Scale", &bloom.upsample_scale, 0.5f, 2.0f,
                       "%.3f");
    ImGui::SliderFloat("Intensity", &bloom.intensity, 0.0f, 20.0f, "%.3f");
    ImGui::SliderFloat("Dirt Intensity", &bloom.dirt_intensity, 0.0f, 5.0f,
                       "%.3f");
  });

  UI::widget("Statistics", [&]() {
    const auto &graphics_command_buffer =
        scene_renderer.get_graphics_command_buffer();
    const auto &compute_command_buffer =
        scene_renderer.get_compute_command_buffer();
    const auto &gpu_times = scene_renderer.get_gpu_execution_times();
    UI::text("Predepth pass: {}ms",
             graphics_command_buffer.get_execution_gpu_time(
                 scene_renderer.get_current_index(), gpu_times.predepth_query));
    UI::text("Sun shadow pass: {}ms",
             graphics_command_buffer.get_execution_gpu_time(
                 scene_renderer.get_current_index(),
                 gpu_times.directional_shadow_pass_query));
    ImGui::Text("Spot Shadow Map Pass: %.3fms",
                graphics_command_buffer.get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.spot_shadow_pass_query));

    /*ImGui::Text("Hierarchical Depth: %.3fms",
                commandBuffer->get_execution_gpu_time(
                    scene_renderer.get_current_index(),
                    gpu_times.HierarchicalDepthQuery));
    ImGui::Text(
        "Pre-Integration: %.3fms",
        commandBuffer->get_execution_gpu_time(
            scene_renderer.get_current_index(),
    gpu_times.PreIntegrationQuery));*/
    UI::text("Light Culling Pass: {}ms",
             compute_command_buffer.get_execution_gpu_time(
                 scene_renderer.get_current_index(),
                 gpu_times.light_culling_pass_query));
    UI::text(
        "Geometry Pass: {}ms",
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
    UI::text("Bloom Pass: {}ms", compute_command_buffer.get_execution_gpu_time(
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
    UI::text("Composite Pass: {}ms",
             graphics_command_buffer.get_execution_gpu_time(
                 scene_renderer.get_current_index(),
                 gpu_times.composite_pass_query));

    const PipelineStatistics &pipeline_stats =
        graphics_command_buffer.get_pipeline_statistics(
            scene_renderer.get_current_index());
    UI::text("Input Assembly Vertices: {}",
             pipeline_stats.input_assembly_vertices);
    UI::text("Input Assembly Primitives: {}",
             pipeline_stats.input_assembly_primitives);
    UI::text("Vertex Shader Invocations: {}", pipeline_stats.vs_invocations);
    UI::text("Clipping Invocations: {}", pipeline_stats.clip_invocations);
    UI::text("Clipping Primitives: {}", pipeline_stats.clip_primitives);
    UI::text("Fragment Shader Invocations: {}", pipeline_stats.fs_invocations);

    const PipelineStatistics &compute_pipeline_statistics =
        compute_command_buffer.get_pipeline_statistics(
            scene_renderer.get_current_index());
    UI::text("Compute Shader Invocations: {}",
             compute_pipeline_statistics.cs_invocations);
  });

  if (load_entity()) {
    UI::widget(
        "Help", +[]() {
          UI::text("T for Translation");
          UI::text("R for Rotation");
          UI::text("S for Scale");
        });
  }

  central_toolbar();

  for (const auto &widget : widgets)
    widget->on_interface(system);
}

auto ClientApp::on_resize(const Extent<u32> &new_extent) -> void {
  Extent ext = new_extent;
  while (ext.width == 0 || ext.height == 0) {
    get_window()->wait_for_events();
    ext = get_window()->get_extent();
  }

  scene_renderer.on_resize(ext);

  info("New extent: {}", ext);
}

auto ClientApp::delete_selected_entity() -> void {
  auto entity = load_entity();
  if (!entity)
    return;

  // Take a copy of the name
  const auto name = entity->get_name();
  if (active_scene->delete_entity(entity->get_id())) {
    UI::Toast::success(3000, "Entity '{}' deleted successfully!", name);
  }

  selected_entity.reset();
}

auto ClientApp::load_entity() -> std::optional<ECS::Entity> {
  if (!selected_entity)
    return std::nullopt;

  auto entity = ECS::Entity{active_scene.get(), *selected_entity};
  if (!entity.valid()) {
    return std::nullopt;
  }
  return entity;
}

static auto cast_ray(const glm::mat4 &projection, const glm::mat4 &view,
                     const glm::vec3 &camera_position, float mx, float my)
    -> std::pair<glm::vec3, glm::vec3> {
  glm::vec4 mouse_clip_pos = {
      mx,
      my,
      -1.0f,
      1.0f,
  };

  auto copy = projection;
  copy[1][1] *= -1;
  auto inverse_proj = glm::inverse(copy);
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
  auto viewport_width = bounds[1].x - bounds[0].x;
  auto viewport_height = bounds[1].y - bounds[0].y;

  return {
      (x / viewport_width) * 2.0f - 1.0f,
      ((y / viewport_height) * 2.0f - 1.0f) * -1.0f,
  };
}

auto ClientApp::pick_object(const glm::vec3 &origin, const glm::vec3 &direction)
    -> entt::entity {
  entt::entity closest = entt::null;
  float closest_value = std::numeric_limits<float>::max();
  auto view = active_scene->view<ECS::TransformComponent, ECS::MeshComponent>();
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

  for (auto &&[entity, transform_component, component] :
       active_scene->view<ECS::TransformComponent, ECS::GeometryComponent>()
           .each()) {
    glm::mat4 transform = transform_component.compute();
    Ray ray = {
        glm::inverse(transform) * glm::vec4(origin, 1.0f),
        glm::inverse(glm::mat3(transform)) * direction,
    };

    float t{};
    bool intersects = ray.intersects_aabb(
        get_aabb_for_geometry(component.parameters, transform), t);
    if (intersects) {
      if (t < closest_value) {
        closest_value = t;
        closest = entity;
      }
    }
  }
  return closest;
}

auto ClientApp::copy_selected_entity() -> void {
  if (!selected_entity)
    return;

  auto entity = ECS::Entity{active_scene.get(), *selected_entity};
  if (!entity.valid())
    return;

  auto copy = active_scene->create_entity("");

  ECS::SceneSerialiser serialiser{};
  std::stringstream stream{};
  if (!stream)
    return;

  if (!serialiser.serialise_entity_components(stream,
                                              ECS::ImmutableEntity{entity})) {
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

  active_scene->initialise_device_dependent_objects(*get_device());
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
      active_scene->clear();
      return true;
    }

    if (event.get_keycode() == KeyCode::KEY_N &&
        Input::pressed<KeyCode::KEY_LEFT_CONTROL>()) {
      if (active_scene->save()) {
        active_scene->clear();
      }
      return true;
    }

    if (event.get_keycode() == KeyCode::KEY_DELETE) {
      delete_selected_entity();
      return true;
    }
    if (scene_state_fsm.get_current_state() == SceneState::Play &&
        Input::pressed(KeyCode::KEY_LEFT_ALT)) {
      if (event.get_repeat_count() == 0) {
        switch (static_cast<KeyCode>(event.get_keycode())) {
        case KeyCode::KEY_C:
          editor_camera_in_runtime = !editor_camera_in_runtime;
          break;
        default:
          break;
        }
      }
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
        if (ImGuizmo::IsUsingAny())
          return false;

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
        if (maybe != entt::null && !ImGuizmo::IsUsingAny()) {
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

  auto little_tokyo = active_scene->create_entity("Whatever");
  auto &mesh_comp = little_tokyo.add_component<ECS::MeshComponent>();
  mesh_comp.mesh =
      Mesh::reference_import_from(*get_device(), FS::model("sphere.gltf"));

  for (int x = 0; x < grid_size; ++x) {
    for (int y = 0; y < grid_size; ++y) {
      for (int z = 0; z < grid_size; ++z) {
        std::string entity_name = "Cube_" + std::to_string(x) + "_" +
                                  std::to_string(y) + "_" + std::to_string(z);
        auto entity = active_scene->create_entity(entity_name);

        float gradient_factor =
            static_cast<float>(x + y + z) / (grid_size * 3 - 3);
        glm::vec3 gradient_color = color_start * (1.0f - gradient_factor) +
                                   color_end * gradient_factor;

        auto &transform = entity.get_transform();
        transform.position = glm::vec3(x * spacing, y * spacing, z * spacing);

        auto &geom = entity.add_component<ECS::GeometryComponent>();
        float randomSize = Random::as_float(0.5, 100.0F);
        geom.parameters = ECS::BasicGeometry::CubeParameters{randomSize};

        auto randomSize3 = Random::vec3(0.5, 100.0F);
        entity.get_transform().scale = randomSize3;

        entity.add_component<ECS::TextureComponent>(
            glm::vec4{gradient_color, 1.0F});
        // entity.add_component<ECS::MeshComponent>(Mesh::reference_import_from(
        //     *get_device(), FS::model("sphere.gltf")));
      }
    }
  }

  active_scene->sort();
}

void ClientApp::on_scene_play() {
  selected_entity.reset();
  scene_state_fsm.transition_to(SceneState::Play);

  runtime_scene = make_scope<ECS::Scene>("Default");
  active_scene->copy_to(*runtime_scene);
  // runtime_scene->SetSceneTransitionCallback(
  //     [this](const std::string &scene) { QueueSceneTransition(scene); });
  // ScriptEngine::SetSceneContext(runtime_scene, m_ViewportRenderer);
  // AssetEditorPanel::SetSceneContext(runtime_scene);
  runtime_scene->on_runtime_start();
  active_scene.swap(runtime_scene);
  set_scene_context();
}

void ClientApp::on_scene_stop() {
  selected_entity.reset();

  runtime_scene->on_runtime_stop();
  scene_state_fsm.transition_to(SceneState::Edit);
  scene_renderer.set_opacity(1.0f);

  // Unload runtime scene
  runtime_scene = nullptr;

  // ScriptEngine::SetSceneContext(editor_scene, m_ViewportRenderer);
  // AssetEditorPanel::SetSceneContext(editor_scene);
  // MiniAudioEngine::SetSceneContext(editor_scene);
  active_scene = editor_scene;
  active_scene->initialise_device_dependent_objects(*get_device());
  set_scene_context();
}

void ClientApp::on_scene_start_simulation() {
  selected_entity.reset();

  scene_state_fsm.transition_to(SceneState::Simulate);

  simulation_scene.reset(new ECS::Scene("Simulation"));
  editor_scene->copy_to(*simulation_scene);

  // AssetEditorPanel::SetSceneContext(simulation_scene);
  simulation_scene->on_simulation_start();
  active_scene.swap(simulation_scene);
  set_scene_context();
}

void ClientApp::on_scene_stop_simulation() {
  selected_entity.reset();

  simulation_scene->on_simulation_stop();
  scene_state_fsm.transition_to(SceneState::Edit);
  simulation_scene.reset();
  active_scene.swap(editor_scene);
  set_scene_context();
}

static inline ImRect RectExpanded(const ImRect &rect, float x, float y) {
  ImRect result = rect;
  result.Min.x -= x;
  result.Min.y -= y;
  result.Max.x += x;
  result.Max.y += y;
  return result;
}

void ClientApp::central_toolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  auto &colors = ImGui::GetStyle().Colors;
  const auto &buttonHovered = colors[ImGuiCol_ButtonHovered];
  ImGui::PushStyleColor(
      ImGuiCol_ButtonHovered,
      ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
  const auto &buttonActive = colors[ImGuiCol_ButtonActive];
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
      ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

  ImGui::Begin("##toolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  bool toolbarEnabled = active_scene != nullptr;

  ImVec4 tintColor = ImVec4(1, 1, 1, 1);
  if (!toolbarEnabled)
    tintColor.w = 0.5f;

  float size = ImGui::GetWindowHeight() - 4.0f;
  ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) -
                       (size * 0.5f));

  const auto current_scene_state = scene_state_fsm.get_current_state();
  bool has_play_button = current_scene_state == SceneState::Edit ||
                         current_scene_state == SceneState::Play;
  bool has_simulate_button = current_scene_state == SceneState::Edit ||
                             current_scene_state == SceneState::Simulate;
  bool has_pause_button = current_scene_state != SceneState::Edit;

  if (has_play_button) {
    auto &icon = (current_scene_state == SceneState::Edit ||
                  current_scene_state == SceneState::Simulate)
                     ? *play_icon
                     : *stop_icon;
    if (UI::image_button(icon) && toolbarEnabled) {
      if (current_scene_state == SceneState::Edit ||
          current_scene_state == SceneState::Simulate) {
        on_scene_play();
      } else if (current_scene_state == SceneState::Play) {
        on_scene_stop();
      }
    }
  }

  if (has_simulate_button) {
    if (has_play_button)
      ImGui::SameLine();

    auto &icon = (current_scene_state == SceneState::Edit ||
                  current_scene_state == SceneState::Play)
                     ? *simulate_icon
                     : *stop_icon;
    if (UI::image_button(icon) && toolbarEnabled) {
      if (current_scene_state == SceneState::Edit ||
          current_scene_state == SceneState::Play) {
        on_scene_start_simulation();
      } else if (current_scene_state == SceneState::Simulate) {
        on_scene_stop_simulation();
      }
    }
  }
  /*
  if (has_pause_button) {
    bool isPaused = active_scene->is_paused();
    ImGui::SameLine();
    {
      Ref<Texture2D> icon = m_IconPause;
      if (ImGui::ImageButton((ImTextureID)(uint64_t)icon->GetRendererID(),
                             ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), 0,
                             ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor) &&
          toolbarEnabled) {
        m_ActiveScene->SetPaused(!isPaused);
      }
    }

    // Step button
    if (isPaused) {
      ImGui::SameLine();
      {
        Ref<Texture2D> icon = m_IconStep;
        bool isPaused = m_ActiveScene->IsPaused();
        if (ImGui::ImageButton((ImTextureID)(uint64_t)icon->GetRendererID(),
                               ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
                               0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor) &&
            toolbarEnabled) {
          m_ActiveScene->Step();
        }
      }
    }
  }
  */
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
  ImGui::End();
}
