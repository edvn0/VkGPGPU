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

template <std::size_t N = 10000> struct FPSAverage {
  std::array<Core::floating, N> frame_times{};
  Core::floating frame_time_sum = 0.0;
  Core::i32 frame_time_index = 0;
  Core::i32 frame_counter = 0;

  std::chrono::high_resolution_clock::time_point last_time;
  bool initialized = false;

  auto update() -> void {
    if (!initialized) {
      last_time = std::chrono::high_resolution_clock::now();
      initialized = true;
      return;
    }

    const auto current_time = std::chrono::high_resolution_clock::now();
    const auto delta_time_seconds =
        std::chrono::duration<Core::floating>(current_time - last_time).count();
    last_time = current_time;

    // Update moving average for frame time
    frame_time_sum -= frame_times[frame_time_index];
    frame_times[frame_time_index] = delta_time_seconds;
    frame_time_sum += delta_time_seconds;
    frame_time_index = (frame_time_index + 1) % N;

    // Increment frame counter
    frame_counter++;
  }

  [[nodiscard]] auto should_print() const -> bool {
    return (frame_counter + 1) % N == 0;
  }

  auto print() const -> void {
    const auto avg_frame_time = frame_time_sum / N;
    const auto fps = 1.0 / avg_frame_time;

    // Log average frame time and FPS
    info("Average Frame Time: {:.6f} ms, FPS: {:.0f}", avg_frame_time * 1000.0,
         fps);
  }
};

namespace Core {

App::App(const ApplicationProperties &props) : properties(props) {
  // Initialize the instance
  instance = Instance::construct(props.headless);

  const auto hostname = "localhost";
  const auto port = 5672;
  message_client = make_scope<Bus::MessagingClient>(
      make_scope<Platform::RabbitMQ::RabbitMQMessagingAPI>(hostname, port));

  Extent<u32> extent{1280, 720};

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
    FPSAverage<1000> fps_average;
    on_create();

    auto last_time = now();
    const auto total_time = last_time;

    while (!window->should_close()) {
      device->get_descriptor_resource()->begin_frame(
          swapchain->current_frame());
      const auto current_time = now();

      const auto delta_time_seconds =
          std::chrono::duration<floating>(current_time - last_time).count();

      fps_average.update();

      if (fps_average.should_print()) {
        fps_average.print();
      }
      swapchain->begin_frame();
      { on_update(delta_time_seconds); }

      {
        interface_system->begin_frame();
        on_interface(*interface_system);
        interface_system->end_frame();
      }

      swapchain->end_frame();
      swapchain->present();

      last_time = current_time;

      frame_counter++;

      window->update();
      device->get_descriptor_resource()->end_frame();

#ifndef GPGPU_RELEASE
      // We want to make sure that all the destruction logic is working :^)
      if (frame_counter > 2'000) {
        break;
      }
#endif
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
