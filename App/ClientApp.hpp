#pragma once

#include "Allocator.hpp"
#include "App.hpp"
#include "Buffer.hpp"
#include "BufferSet.hpp"
#include "Camera.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorResource.hpp"
#include "Destructors.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "FiniteStateMachine.hpp"
#include "Framebuffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Material.hpp"
#include "Math.hpp"
#include "Mesh.hpp"
#include "Pipeline.hpp"
#include "SceneRenderer.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Timer.hpp"
#include "Types.hpp"

#include <compare>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <random>
#include <unordered_map>

#include "bus/MessagingClient.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Scene.hpp"
#include "widgets/Widget.hpp"

using namespace Core;

enum class GuizmoOperation : std::uint16_t {
  Tx = (1u << 0),
  Ty = (1u << 1),
  Tz = (1u << 2),
  Rx = (1u << 3),
  Ry = (1u << 4),
  Rz = (1u << 5),
  Rscreen = (1u << 6),
  Sx = (1u << 7),
  Sy = (1u << 8),
  Sz = (1u << 9),
  Bounds = (1u << 10),
  Sxu = (1u << 11),
  Syu = (1u << 12),
  Szu = (1u << 13),

  T = Tx | Ty | Tz,
  R = Rx | Ry | Rz,
  S = Sx | Sy | Sz,
  Su = Sxu | Syu | Szu,
  UNIVERSAL = T | R | Su
};

class SceneWidget;

class ClientApp : public App {
public:
  explicit ClientApp(const ApplicationProperties &props);
  ~ClientApp() override = default;

  void on_update(floating ts) override;
  void on_resize(const Extent<u32> &) override;
  void on_interface(Core::InterfaceSystem &) override;
  void on_create() override;
  void on_destroy() override;
  void on_event(Event &) override;

private:
  Ref<ECS::Scene> editor_scene;
  Ref<ECS::Scene> active_scene;
  Ref<ECS::Scene> simulation_scene;
  Ref<ECS::Scene> runtime_scene;

  // Non owning :) owned by the parent App.
  std::vector<ISceneContextDependent *> scene_context_dependents;

  std::optional<entt::entity> selected_entity;
  std::tuple<u32, u32> main_position{};
  Extent<float> main_size{};
  GuizmoOperation current_operation{GuizmoOperation::T};

  EditorCamera camera;

  std::vector<Scope<Widget>> widgets{};
  auto notify_all(const ECS::Message &message) const {
    for (auto &widget : widgets) {
      widget->on_notify(message);
    }
  }

  Scope<Texture> play_icon;
  Scope<Texture> pause_icon;
  Scope<Texture> simulate_icon;
  Scope<Texture> stop_icon;

  std::array<glm::vec2, 2> viewport_bounds{};
  bool camera_can_receive_events{true};
  bool viewport_mouse_hovered{false};
  bool viewport_focused{false};
  bool right_click_started_in_viewport{false};
  bool editor_camera_in_runtime{false};

  enum class SceneState : Core::u8 {
    Edit = 0,
    Play = 1,
    Pause = 2,
    Simulate = 3,
  };
  FiniteStateMachine<SceneState> scene_state_fsm{SceneState::Edit};

  auto load_entity() -> std::optional<ECS::Entity>;
  auto copy_selected_entity() -> void;
  auto delete_selected_entity() -> void;
  [[nodiscard]] auto pick_object(const glm::vec3 &, const glm::vec3 &)
      -> entt::entity;

  Timer timer;

  SceneRenderer scene_renderer;

  void create_dummy_scene();

  void on_scene_play();
  void on_scene_stop();
  void on_scene_start_simulation();
  void on_scene_stop_simulation();

  auto central_toolbar() -> void;

  auto set_scene_context() {
    for (auto &dependent : scene_context_dependents) {
      dependent->set_scene_context(active_scene.get());
    }
  }
};
