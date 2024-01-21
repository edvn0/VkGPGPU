#pragma once

#include <string>
#include <string_view>

namespace Core::UI::Platform {

auto accept_drag_drop_payload(std::string_view) -> std::string;

} // namespace Core::UI::Platform
