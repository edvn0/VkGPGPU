#pragma once

#include "DescriptorResource.hpp"
#include "Device.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Instance.hpp"
#include "Types.hpp"

#include "bus/MessagingClient.hpp"

namespace Core {

struct ApplicationProperties {
  bool headless{true};
};

class App {
public:
  auto run() -> void;
  virtual ~App();

protected:
  virtual auto on_update(floating ts) -> void = 0;
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

private:
  Scope<Instance> instance;
  Scope<Device> device;
  Scope<Bus::MessagingClient> message_client;

  ApplicationProperties properties{};

  u32 current_frame{0};
  bool running{true};
  u64 frame_counter{0};
};

auto extern make_application(const ApplicationProperties &) -> Scope<App>;

} // namespace Core
