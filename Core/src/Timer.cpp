#include "pch/vkgpgpu_pch.hpp"

#include "Timer.hpp"

#include <fmt/format.h>
#include <fstream>

#ifndef GPGPU_RETAIN_SECONDS
constexpr auto GPGPU_BUFFER_SECONDS = 0.5;
#endif

namespace Core {

Timer::Timer(const Bus::MessagingClient &client) : messaging_client(&client) {
  worker_thread =
      std::jthread([this](std::stop_token st) { process_timings(st); });
}

Timer::~Timer() {
  if (worker_thread.joinable()) {
    worker_thread.request_stop(); // Signals the thread to stop
    queue_cv.notify_one();        // Wake the thread to ensure it can exit
    worker_thread.join();         // Wait for the thread to finish
  }
}

void Timer::begin() { start_time = std::chrono::high_resolution_clock::now(); }

void Timer::end() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                      end_time - start_time)
                      .count();

  {
    std::lock_guard lock(queue_mutex);
    timings_queue.push(duration);
  }

  queue_cv.notify_one();
}

void Timer::process_timings(std::stop_token stop_token) {
  std::unique_lock lock(queue_mutex, std::defer_lock);

  while (!stop_token.stop_requested()) {
    lock.lock();
    queue_cv.wait(lock, [&queue = timings_queue, &stop = stop_token]() {
      return !queue.empty() || stop.stop_requested();
    });

    while (!timings_queue.empty()) {
      auto duration = timings_queue.front();
      timings_queue.pop();
      lock.unlock();
      write_to_file(duration);
      lock.lock();
    }

    lock.unlock();
  }
}

void Timer::write_to_file(BufferSize duration) {
  // Implementation for writing a single duration to a file or sending via
  // messaging client
  auto formatted = fmt::format("Time Taken (microseconds): {}", duration);
  messaging_client->send_message("Timer", formatted);
}

} // namespace Core
