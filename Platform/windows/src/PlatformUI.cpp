#include "PlatformUI.hpp"

#include <Windows.h>
#include <imgui.h>

namespace Core::UI::Platform {

std::string convert_to_standard_string(const wchar_t *wideString) {
  if (!wideString)
    return std::string();
  int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, nullptr, 0,
                                       nullptr, nullptr);
  std::string narrowString(bufferSize, 0);
  WideCharToMultiByte(CP_UTF8, 0, wideString, -1, &narrowString[0], bufferSize,
                      nullptr, nullptr);
  return narrowString;
}

std::string accept_drag_drop_payload(std::string_view payload_type) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
            payload_type.data(), ImGuiDragDropFlags_SourceAllowNullID)) {
      std::string payloadData = convert_to_standard_string(
          reinterpret_cast<const wchar_t *>(payload->Data));
      ImGui::EndDragDropTarget();
      return payloadData;
    }
    ImGui::EndDragDropTarget();
  }
  return {}; // Return empty string if no payload is accepted
}

} // namespace Core::UI::Platform
