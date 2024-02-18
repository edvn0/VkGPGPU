static constexpr auto draw_camera_type = [](auto &component) {
  auto copy = component.camera_type;

  std::string items;
  for (auto item : magic_enum::enum_values<Core::CameraType>()) {
    items += magic_enum::enum_name(item);
    items += '\0';
  }
  items += '\0';

  auto current_index = static_cast<i32>(copy);

  if (ImGui::Combo("Camera type", &current_index, items.c_str())) {
    copy = static_cast<Core::CameraType>(current_index);
  }

  const auto selectedName = magic_enum::enum_name(copy);
  ImGui::Text("Selected Item: %s", selectedName.data());

  if (copy != component.camera_type) {
    component.camera_type = copy;
  }
};

draw_camera_type(component);
