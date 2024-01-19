#pragma once

#include "DescriptorResource.hpp"
#include "Device.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Instance.hpp"
#include "Swapchain.hpp"
#include "Types.hpp"
#include "Window.hpp"

#include "bus/MessagingClient.hpp"

namespace Core {

class InterfaceSystem;

struct ApplicationProperties {
  bool headless{true};
};

class App {
public:
  auto run() -> void;
  virtual ~App();

protected:
  virtual auto on_update(floating ts) -> void = 0;
  virtual auto on_interface(InterfaceSystem &) -> void = 0;
  virtual auto on_create() -> void = 0;
  virtual auto on_destroy() -> void = 0;

  explicit App(const ApplicationProperties &);

  [[nodiscard]] auto frame() const -> u32;

  [[nodiscard]] auto get_device() const -> const Scope<Device> & {
    return device;
  }
  [[nodiscard]] auto get_messaging_client() const
      -> const Scope<Bus::MessagingClient> & {
    return message_client;
  }
  [[nodiscard]] auto get_window() const -> const Scope<Window> & {
    return window;
  }
  [[nodiscard]] auto get_swapchain() const -> const Scope<Swapchain> & {
    return swapchain;
  }

  [[nodiscard]] auto get_instance() const -> const Scope<Instance> & {
    return instance;
  }

  [[nodiscard]] auto get_frame_counter() const -> u64 { return frame_counter; }

private:
  Scope<Instance> instance;
  Scope<Device> device;
  Scope<Bus::MessagingClient> message_client;
  Scope<Window> window;
  Scope<Swapchain> swapchain;

  ApplicationProperties properties{};

  u64 frame_counter{0};
  void interface_pass();
};

auto extern make_application(const ApplicationProperties &) -> Scope<App>;

} // namespace Core
