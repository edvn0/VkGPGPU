#include "SceneWidget.hpp"

#include "Device.hpp"
#include "UI.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"

using namespace Core;
using namespace ECS;

SceneWidget::SceneWidget(const Device &device) : device(&device) {}

void SceneWidget::on_interface(InterfaceSystem &) {
  if (!UI::begin("SceneContext")) {
    UI::end();
    return;
  }

  const auto identifer_view = context->get_registry().view<IdentityComponent>();
  // Make a list of all the entities in the scene, should be selectable
  static entt::entity selected = entt::null;
  static std::string selected_name;

  for (const auto entity : identifer_view) {
    auto &identity = identifer_view.get<IdentityComponent>(entity);
    ImGui::PushID(static_cast<i32>(identity.id));
    bool is_selected = selected == entity;
    if (ImGui::Selectable(identity.name.c_str(), &is_selected)) {
      selected = entity;
      selected_name = identity.name;
    }
    ImGui::PopID();
  }

  // If i click outside of the list, deselect the entity
  // Must also be inside the SceneContext window
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
    selected = entt::null;
    selected_name.clear();
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

  if (selected != entt::null) {
    Entity entity{
        context,
        selected,
        selected_name,
    };

    draw_component_widget(entity);
  } else {
    UI::text("No entity selected");
  }

  UI::end();
}

void SceneWidget::draw_component_widget(ECS::Entity &entity) {
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
    draw_add_component_all(entity, ECS::EngineComponents{});
    ImGui::EndPopup();
  }

  ImGui::PopItemWidth();

  draw_component<TransformComponent>(entity, "Transform", [](auto &transform) {
    ImGui::DragFloat3("Position", &transform.position.x, 0.1F);
    ImGui::DragFloat4("Rotation", &transform.rotation.x, 0.1F);
    ImGui::DragFloat3("Scale", &transform.scale.x, 0.1F);
  });

  draw_component<TextureComponent>(
      entity, "Texture", [](TextureComponent &texture) {
        auto &texture_vector4 = texture.colour;
        ImGui::ColorEdit4("Colour", &texture_vector4.x);
      });
}
