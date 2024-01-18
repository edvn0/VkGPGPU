#include "pch/vkgpgpu_pch.hpp"

#include "Timer.hpp"

#include <fmt/format.h>
#include <fstream>

#ifndef GPGPU_RETAIN_SECONDS
constexpr auto GPGPU_BUFFER_SECONDS = 0.5;
#endif

namespace Core {

std::vector<Timer::BufferSize> Timer::buffer(Timer::buffer_size);
std::chrono::time_point<std::chrono::high_resolution_clock>
    Timer::last_write_time = std::chrono::high_resolution_clock::now();
size_t Timer::current_index = 0;
size_t Timer::last_write_index = 0;
std::mutex Timer::buffer_mutex;
static constexpr auto loops_per_second = 20000;
const size_t Timer::buffer_size = GPGPU_BUFFER_SECONDS * loops_per_second;

Timer::Timer(const Bus::MessagingClient &client) : messaging_client(&client) {
  start_time = std::chrono::high_resolution_clock::now();
}

Timer::~Timer() { write_to_file(); }

void Timer::begin() { start_time = std::chrono::high_resolution_clock::now(); }

void Timer::end() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                      end_time - start_time)
                      .count();
  add_duration(duration);
  if (should_write_to_file()) {
  }
}

void Timer::add_duration(Timer::BufferSize duration) {
  std::lock_guard lock(buffer_mutex);
  buffer[current_index] = duration;
  current_index = (current_index + 1) % buffer_size;
}

bool Timer::should_write_to_file() {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_write_time)
                     .count();
  return elapsed >= 200;
}

void Timer::write_to_file() {
  std::lock_guard lock(buffer_mutex);

  size_t index = last_write_index;
  while (index != current_index) {
    auto formatted =
        fmt::format("Time Taken (microseconds), {}", buffer[index]);
    messaging_client->send_message("Timer", formatted);
    last_write_time = std::chrono::high_resolution_clock::now();
    index = (index + 1) % buffer_size;
  }

  last_write_index = current_index; // Update the last write index after writing
}

} // namespace Core
