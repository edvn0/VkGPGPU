#pragma once

#include "Instance.hpp"
#include "Window.hpp"

class MockWindow : public Core::Window {
public:
  MockWindow(const Core::Instance &instance)
      : Core::Window(instance, {
                                   .headless = true,
                               }) {}
  ~MockWindow() override = default;
};
