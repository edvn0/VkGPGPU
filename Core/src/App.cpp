#include "pch/vkgpgpu_pch.hpp"

#include "App.hpp"

#include "Allocator.hpp"
#include "Config.hpp"
#include "DescriptorResource.hpp"
#include "Formatters.hpp"
#include "InterfaceSystem.hpp"
#include "Logger.hpp"

#include <cstddef>
#include <exception>

#include "rabbitmq/RabbitMQMessagingAPI.hpp"

namespace Core {

auto AppDeleter::operator()(App *app) const noexcept -> void {
  delete app;
  info("Everything is cleaned up. Goodbye!");
}

App::App(const ApplicationProperties &props) : properties(props) {
  // Initialize the instance
  instance = Instance::construct(properties.headless);

  const auto hostname = "localhost";
  const auto port = 5672;
  message_client = make_scope<Bus::MessagingClient>(
      make_scope<Platform::RabbitMQ::RabbitMQMessagingAPI>(hostname, port));

  window = Window::construct(*instance, {
                                            .extent = extent,
                                            .fullscreen = false,
                                            .vsync = false,
                                            .headless = properties.headless,
                                        });
  // Initialize the device
  device = Device::construct(*instance, *window);

  swapchain = Swapchain::construct(*device, *window,
                                   {
                                       .extent = extent,
                                       .headless = properties.headless,
                                   });
  Allocator::construct(*device, *instance);
}

App::~App() {
  Allocator::destroy();
  swapchain.reset();
  window.reset();
  device.reset();
  instance.reset();
}

auto App::frame() const -> u32 { return swapchain->current_frame(); }

auto App::run() -> void {
  static constexpr auto now = [] {
    return std::chrono::high_resolution_clock::now();
  };

  Scope<InterfaceSystem> interface_system =
      make_scope<InterfaceSystem>(*device, *window, *swapchain);

  try {
    on_create();

    auto last_time = now();
    const auto total_time = last_time;

    while (!window->should_close()) {
      if (!swapchain->begin_frame())
        continue;

      fps_average.update();

      if (fps_average.should_print()) {
        fps_average.print();
      }

      device->get_descriptor_resource()->begin_frame(
          swapchain->current_frame());
      const auto current_time = now();

      const auto delta_time_seconds =
          std::chrono::duration<floating>(current_time - last_time).count();

      on_update(delta_time_seconds);

      {
        interface_system->begin_frame();
        on_interface(*interface_system);
        interface_system->end_frame();
      }

      swapchain->end_frame();

      last_time = current_time;

      frame_counter++;

      window->update();

      device->get_descriptor_resource()->end_frame();
      swapchain->present();
    }

    vkDeviceWaitIdle(device->get_device());

    info("Total time: {} seconds.",
         std::chrono::duration<floating>(now() - total_time).count());

    on_destroy();

  } catch (const std::exception &exc) {
    error("Main loop exception: {}", exc);
    throw;
  }
}

} // namespace Core
