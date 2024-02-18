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
  Scope<ECS::Scene> scene;
  Scope<CommandDispatcher> dispatcher;
  Scope<DynamicLibraryLoader> loader;

  std::optional<entt::entity> selected_entity;

  GuizmoOperation current_operation{GuizmoOperation::T};
  static auto cycle(GuizmoOperation) -> GuizmoOperation;

  EditorCamera camera;

  std::vector<Scope<Widget>> widgets{};
  auto notify_all(const ECS::Message &message) const {
    for (auto &widget : widgets) {
      widget->on_notify(message);
    }
  }

  Timer timer;

  SceneRenderer scene_renderer;

  void create_dummy_scene();
};
