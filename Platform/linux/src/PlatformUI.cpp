#include "PlatformUI.hpp"

#include "Types.hpp"

#include <imgui.h>

namespace Core::UI::Platform {

auto accept_drag_drop_payload(std::string_view payload_type) -> std::string {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
            payload_type.data(), ImGuiDragDropFlags_SourceAllowNullID)) {
      const auto data = static_cast<const char *>(payload->Data);
      const auto size = static_cast<usize>(payload->DataSize);
      return std::string{data, size};
    }
    ImGui::EndDragDropTarget();
  }
  return {}; // Return empty string if no payload is accepted
}

auto set_drag_drop_payload(std::string_view payload_type,
                           const std::string_view data) -> bool {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    const auto as_string = std::string{data};
    const auto size = as_string.size() * sizeof(char);
    ImGui::SetDragDropPayload(payload_type.data(), as_string.c_str(), size);
    ImGui::EndDragDropSource();
  }
  return true;
}

auto set_drag_drop_payload(std::string_view payload_type, const FS::Path &data)
    -> bool {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    const auto as_string = data.string();
    const auto size = as_string.size() * sizeof(char);
    ImGui::SetDragDropPayload(payload_type.data(), as_string.c_str(), size);
    ImGui::EndDragDropSource();
  }
  return true;
}

} // namespace Core::UI::Platform
