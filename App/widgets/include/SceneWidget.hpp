#pragma once

#include "Widget.hpp"

#include <imgui.h>

#include "core/Forward.hpp"
#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

class SceneWidget : public Widget {
public:
  explicit SceneWidget(const Core::Device &);

  auto set_scene_context(ECS::Scene *scene) -> void { context = scene; }

  void on_update(Core::floating ts) override {}
  void on_interface(Core::InterfaceSystem &) override;
  void on_create(const Core::Device &, const Core::Window &,
                 const Core::Swapchain &) override {}
  void on_destroy() override {}

private:
  const Core::Device *device;
  ECS::Scene *context{nullptr};

  void draw_component_widget(ECS::Entity &entity);

  template <typename ToTest, typename... CompareWith>
  auto is_deletable_component(ECS::ComponentList<CompareWith...>) {
    return (std::is_same_v<ToTest, CompareWith> || ...);
  }

  template <typename T, class Func>
  void draw_component(auto &entity, std::string_view name, Func &&ui_function) {
    if (!entity.template has_component<T>()) {
      return;
    }

    ImGui::PushID(reinterpret_cast<void *>(typeid(T).hash_code()));
    ImVec2 content_region_available = ImGui::GetContentRegionAvail();

    bool open =
        ImGui::TreeNodeEx(name.data(), ImGuiTreeNodeFlags_OpenOnDoubleClick);
    bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    float line_height = ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y;

    bool reset_values = false;
    bool remove_component = false;

    ImGui::SameLine(content_region_available.x - line_height - 5.0f);
    if (ImGui::Button("+", ImVec2{line_height, line_height}) || right_clicked) {
      ImGui::OpenPopup("ComponentSettings");
    }

    if (ImGui::BeginPopup("ComponentSettings")) {
      auto &component = entity.template get_component<T>();

      if (ImGui::MenuItem("Reset")) {
        reset_values = true;
      }

      if (!is_deletable_component<T>(ECS::UnremovableComponents{})) {
        if (ImGui::MenuItem("Remove component")) {
          remove_component = true;
        }
      }

      ImGui::EndPopup();
    }

    if (open) {
      T &component = entity.template get_component<T>();
      std::forward<Func>(ui_function)(component);
      ImGui::TreePop();
    }

    if (remove_component) {
      if (entity.template has_component<T>()) {
        entity.template remove_component<T>();
      }
    }

    if (reset_values) {
      if (entity.template has_component<T>()) {
        entity.template remove_component<T>();
        entity.template add_component<T>();
      }
    }

    if (!open) {
      // UI::shift_cursor_y(-(ImGui::GetStyle().ItemSpacing.y + 1.0F));
    }

    ImGui::PopID();
  }

  template <typename T, class Func>
  void draw_component(auto &entity, Func &&ui_function) {
    draw_component<T, Func>(entity, T::component_name,
                            std::forward<Func>(ui_function));
  }

  template <typename T> void draw_add_component_entry(auto &entity) {
    if (!entity.template has_component<T>()) {
      if (ImGui::MenuItem(T::component_name.data())) {
        entity.template add_component<T>();
        ImGui::CloseCurrentPopup();
      }
    }
  }
  template <typename... T>
  void draw_add_component_all(auto &entity, ECS::ComponentList<T...>) {
    (draw_add_component_entry<T>(entity), ...);
  }
};
