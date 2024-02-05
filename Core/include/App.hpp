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

template <Core::usize N = 10000> struct FPSAverage {
  std::array<Core::floating, N> frame_times{};
  Core::floating frame_time_sum = 0.0;
  Core::usize frame_time_index = 0;
  Core::usize frame_counter = 0;

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

  [[nodiscard]] auto get_statistics() const {
    const auto avg_frame_time = frame_time_sum / N;
    const auto fps = 1.0 / avg_frame_time;
    return std::tuple{1000.0F * avg_frame_time, fps};
  }
};

class InterfaceSystem;
class App;
struct AppDeleter {
  auto operator()(App *app) const noexcept -> void;
};

struct ApplicationProperties {
  const bool headless{true};
  const bool start_fullscreen{false};
};

class App {
public:
  auto run() -> void;
  virtual ~App();

protected:
  virtual auto on_update(floating ts) -> void = 0;
  virtual auto on_resize(const Extent<u32> &) -> void = 0;
  virtual auto on_interface(InterfaceSystem &) -> void = 0;
  virtual auto on_create() -> void = 0;
  virtual auto on_destroy() -> void = 0;
  virtual auto on_event(Event &) -> void{};

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

  [[nodiscard]] auto was_resized() const -> bool {
    return window->was_resized();
  }

  [[nodiscard]] auto get_frame_counter() const -> u64 { return frame_counter; }

  [[nodiscard]] auto get_timer() const -> const auto & { return fps_average; };

private:
  Scope<Instance> instance;
  Scope<Device> device;
  Scope<Bus::MessagingClient> message_client;
  Scope<Window> window;
  Scope<Swapchain> swapchain;

  Extent<u32> extent{1280, 720};
  FPSAverage<144> fps_average{};

  ApplicationProperties properties{};

  auto forward_incoming_events(Event &) -> void;

  u64 frame_counter{0};
};

auto extern make_application(const ApplicationProperties &)
    -> Scope<App, AppDeleter>;

} // namespace Core
