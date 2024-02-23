#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#include "bus/MessagingClient.hpp"

namespace Core {

class Timer {
public:
  using BufferSize = long long;

  explicit Timer(const Bus::MessagingClient &);
  ~Timer();

  void begin();
  void end();

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  std::queue<BufferSize> timings_queue;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::jthread worker_thread;
  const Bus::MessagingClient *messaging_client;

  void process_timings(std::stop_token stop_token);
  void write_to_file(BufferSize duration);
};

} // namespace Core
