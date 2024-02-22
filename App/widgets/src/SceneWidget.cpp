#include "SceneWidget.hpp"

#include "Device.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <magic_enum.hpp>
#include <string>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"

using namespace Core;
using namespace ECS;

SceneWidget::SceneWidget(const Device &device,
                         std::optional<entt::entity> &scene_selected)
    : device(&device), selected(scene_selected) {}

void SceneWidget::on_interface(InterfaceSystem &) {
  if (!UI::begin("SceneContext")) {
    UI::end();
    return;
  }

  const auto identifer_view = context->get_registry().view<IdentityComponent>();
  // Make a list of all the entities in the scene, should be selectable

  for (const auto entity : identifer_view) {
    auto &identity = identifer_view.get<IdentityComponent>(entity);
    ImGui::PushID(static_cast<i32>(identity.id));
    bool is_selected = selected.has_value() && selected == entity;

    bool has_children = context->get_registry().any_of<ChildComponent>(entity);
    bool has_parent = context->get_registry().any_of<ParentComponent>(entity);
    if (has_children) {
      const auto &children =
          context->get_registry().get<ChildComponent>(entity);

      if (ImGui::Selectable(identity.name.c_str(), &is_selected)) {
        selected = entity;
        selected_name = identity.name;
      }

      for (const auto child : children.children) {
        const auto &maybe_child_identity = context->get_entity(child);
        if (!maybe_child_identity || !maybe_child_identity->valid())
          break;
        ImGui::Indent();
        ImGui::PushID(static_cast<i32>(child));

        auto child_identity = maybe_child_identity.value();

        bool is_selected =
            selected.has_value() && selected == child_identity.get_handle();
        if (ImGui::Selectable(child_identity.get_name().c_str(),
                              &is_selected)) {
          selected = child_identity.get_handle();
          selected_name = child_identity.get_name();
        }
        ImGui::PopID();
        ImGui::Unindent();
      }
    } else if (!has_parent) {

      ImGui::PushID(static_cast<i32>(identity.id));
      if (ImGui::Selectable(identity.name.c_str(), &is_selected)) {
        selected = entity;
        selected_name = identity.name;
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }

  // Create a right click menu for adding entities
  if (ImGui::BeginPopupContextWindow("AddEntityPopup",
                                     ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::MenuItem("Add Entity")) {
      context->create_entity("New Entity");
    }
    ImGui::EndPopup();
  }

  UI::end();

  // We may now have a selected entity, so we can show the details of it
  if (!UI::begin("EntityDetails")) {
    UI::end();
    return;
  }

  if (selected.has_value() && selected.value() != entt::null) {
    Entity entity{
        context,
        selected.value(),
        selected_name,
    };

    if (last_selected != selected) {

      on_notify(Events::SelectedEntityUpdateEvent{entity.get_id()});
      last_selected = selected.value();
    }

    draw_component_widget(entity);
  } else {
    UI::text("No entity selected");
  }

  UI::end();
}

auto SceneWidget::on_notify(const Message &message) -> void {
  std::visit(
      [this](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Events::SelectedEntityUpdateEvent>) {
          info("Selected entity is now: {}", arg.id);
        }
      },
      message);
}

void SceneWidget::draw_component_widget(Entity &entity) {
  if (entity.has_component<IdentityComponent>()) {
    auto &tag = entity.get_component<IdentityComponent>();
    std::string buffer = tag.name;
    buffer.resize(256);

    if (ImGui::InputText("Tag", buffer.data(), 256,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      buffer.shrink_to_fit();

      if (!buffer.empty() && tag.name != buffer) {
        tag.name = std::string{buffer.c_str()};
      }
    }
  }

  ImGui::SameLine();
  ImGui::PushItemWidth(-1);

  if (ImGui::Button("Add Component")) {
    ImGui::OpenPopup("AddComponent");
  }

  if (ImGui::BeginPopup("AddComponent")) {
    draw_add_component_all(entity, EngineComponents{});
    ImGui::EndPopup();
  }

  ImGui::PopItemWidth();

  draw_component<TransformComponent>(entity, "Transform", [](auto &transform) {
    ImGui::DragFloat3("Position", &transform.position.x, 0.1F);
    ImGui::DragFloat4("Rotation", Math::value_ptr(transform.rotation), 0.1F);
    ImGui::DragFloat3("Scale", &transform.scale.x, 0.1F);
  });

  draw_component<TextureComponent>(
      entity, "Texture", [](TextureComponent &texture) {
        auto &texture_vector4 = texture.colour;
        ImGui::ColorEdit4("Colour", &texture_vector4.x);
      });

  draw_component<CameraComponent>(entity, "Camera",
                                  [](CameraComponent &component) {
#include "inline/camera/camera_type.inl"
                                  });

  draw_component<SunComponent>(entity, "Sun", [](SunComponent &sun) {
    ImGui::DragFloat3("Direction", &sun.direction.x, 0.1F);
    ImGui::ColorEdit4("Colour", &sun.colour.x);
    ImGui::ColorEdit4("Specular Colour", &sun.specular_colour.x);

    Core::DepthParameters &depth = sun.depth_params;
    ImGui::DragFloat("Bias", &depth.bias, 0.1F, 0.0F, 1.0F);
    ImGui::DragFloat("Default Value", &depth.default_value, 0.01F, .01F, 0.2F);
  });

  draw_component<GeometryComponent>(
      entity, "Basic Geometry", [](GeometryComponent &comp) {
        const char *items[] = {"Quad", "Triangle", "Circle", "Sphere", "Cube"};
        int currentItem = comp.parameters.index();

        if (ImGui::Combo("Geometry Type", &currentItem, items,
                         IM_ARRAYSIZE(items))) {
        }

        // Update the variant to reflect the new selection, if changed
        switch (currentItem) {
        case 0: // Quad
          if (!std::holds_alternative<BasicGeometry::QuadParameters>(
                  comp.parameters)) {
            comp.parameters = BasicGeometry::QuadParameters{
                1.0f, 1.0f}; // Default QuadParameters
          }
          break;
        case 1: // Triangle
          if (!std::holds_alternative<BasicGeometry::TriangleParameters>(
                  comp.parameters)) {
            comp.parameters = BasicGeometry::TriangleParameters{
                1.0f, 1.0f}; // Default TriangleParameters
          }
          break;
        case 2: // Circle
          if (!std::holds_alternative<BasicGeometry::CircleParameters>(
                  comp.parameters)) {
            comp.parameters = BasicGeometry::CircleParameters{
                1.0f}; // Default CircleParameters
          }
          break;
        case 3: // Sphere
          if (!std::holds_alternative<BasicGeometry::SphereParameters>(
                  comp.parameters)) {
            comp.parameters = BasicGeometry::SphereParameters{
                1.0f}; // Default SphereParameters
          }
          break;
        case 4: // Cube
          if (!std::holds_alternative<BasicGeometry::CubeParameters>(
                  comp.parameters)) {
            comp.parameters =
                BasicGeometry::CubeParameters{1.0f}; // Default CubeParameters
          }
          break;
        }

        // Draw UI based on the selected Geometry Type
        std::visit(overloaded{[](BasicGeometry::QuadParameters &quad) {
                                ImGui::Text("Quad Parameters");
                                ImGui::DragFloat("Width", &quad.width, 0.1f,
                                                 0.0f, FLT_MAX, "%.3f");
                                ImGui::DragFloat("Height", &quad.height, 0.1f,
                                                 0.0f, FLT_MAX, "%.3f");
                              },
                              [](BasicGeometry::TriangleParameters &triangle) {
                                ImGui::Text("Triangle Parameters");
                                ImGui::DragFloat("Base", &triangle.base, 0.1f,
                                                 0.0f, FLT_MAX, "%.3f");
                                ImGui::DragFloat("Height", &triangle.height,
                                                 0.1f, 0.0f, FLT_MAX, "%.3f");
                              },
                              [](BasicGeometry::CircleParameters &circle) {
                                ImGui::Text("Circle Parameters");
                                ImGui::DragFloat("Radius", &circle.radius, 0.1f,
                                                 0.0f, FLT_MAX, "%.3f");
                              },
                              [](BasicGeometry::SphereParameters &sphere) {
                                ImGui::Text("Sphere Parameters");
                                ImGui::DragFloat("Radius", &sphere.radius, 0.1f,
                                                 0.0f, FLT_MAX, "%.3f");
                              },
                              [](BasicGeometry::CubeParameters &cube) {
                                ImGui::Text("Cube Parameters");
                                ImGui::DragFloat("Side Length",
                                                 &cube.side_length, 0.1f, 0.0f,
                                                 FLT_MAX, "%.3f");
                              }},
                   comp.parameters);
      });

  draw_component<MeshComponent>(
      entity, "Mesh", [&dev = device](MeshComponent &mesh) {
        if (!mesh.path.empty()) {
          UI::text("Path: {}", mesh.path.string());
        } else if (mesh.mesh != nullptr) {
          UI::text("Path: {}", mesh.mesh->get_file_path().string());
        } else {
          UI::text("No mesh selected");
        }

        if (ImGui::Checkbox("AABB", &mesh.draw_aabb)) {
        }

        ImGui::NewLine();
        // Dragdroppable button
        if (ImGui::Button("LoadMesh")) {
        }

        if (const auto path = UI::accept_drag_drop_payload("LoadMesh");
            !path.empty()) {
          mesh.path = path;
          if (const auto constructed =
                  Mesh::reference_import_from(*dev, path)) {
            mesh.mesh = constructed;
          } else {
            info("Failed to load mesh from path: {}", path);
          }
        }
      });
}
