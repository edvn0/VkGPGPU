#pragma once

#include "Filesystem.hpp"

#include <string>
#include <string_view>

namespace Core::UI::Platform {

auto accept_drag_drop_payload(std::string_view) -> std::string;
auto set_drag_drop_payload(std::string_view, std::string_view) -> bool;
auto set_drag_drop_payload(std::string_view, const FS::Path &) -> bool;

} // namespace Core::UI::Platform
