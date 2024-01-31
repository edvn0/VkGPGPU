#pragma once

#include "InterfaceSystem.hpp"
#include "Types.hpp"

#include "core/Forward.hpp"

class Widget {
public:
  virtual ~Widget() = default;
  virtual void on_update(Core::floating ts) = 0;
  virtual void on_interface(Core::InterfaceSystem &) = 0;
  virtual void on_create(const Core::Device &, const Core::Window &,
                         const Core::Swapchain &) = 0;
  virtual void on_destroy() = 0;
};
