#pragma once

#include "CommandBuffer.hpp"
#include "Types.hpp"

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <vulkan/vulkan.h>

namespace Core {

template <class Resource> class CommandBufferThreadPool {
public:
  using TaskType = std::function<Scope<Resource>(CommandBuffer &)>;

  CommandBufferThreadPool(size_t thread_count, const Device &dev)
      : device(&dev) {
    command_buffers.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
      command_buffers.emplace_back(CommandBuffer::construct(
          *device, CommandBufferProperties{
                       .queue_type = Queue::Type::Transfer,
                       .count = 1,
                       .is_primary = true,
                       .owned_by_swapchain = false,
                       .record_stats = false,
                       .mutex_around_queue = true,
                   }));
      threads.emplace_back(&CommandBufferThreadPool::worker_thread, this, i);
    }
  }

  ~CommandBufferThreadPool() { stop_and_join_threads(); }

  auto submit(TaskType task) -> std::future<Scope<Resource>> {
    auto promise = std::make_shared<std::promise<Scope<Resource>>>();
    auto future = promise->get_future();

    {
      std::lock_guard lock(queue_mutex);
      tasks.emplace([moved_task = std::move(task),
                     moved_promise = std::move(promise),
                     this](usize command_buffer_index) {
        moved_promise->set_value(
            moved_task(*command_buffers.at(command_buffer_index)));
      });
    }

    condition_variable.notify_one();
    return future;
  }

private:
  void worker_thread(usize index) {
    while (true) {
      std::function<void(usize)> task;

      {
        std::unique_lock lock(queue_mutex);
        condition_variable.wait(lock,
                                [this] { return stop || !tasks.empty(); });
        if (stop && tasks.empty())
          break;

        task = std::move(tasks.front());
        tasks.pop();
      }

      auto &cmd_buffer = command_buffers[index];

      {
        std::unique_lock lock{other_mutex}; // Lock the VkQueue
        cmd_buffer->begin(0);
        task(index);
        cmd_buffer->end_and_submit();
      }
    }
  }

  void stop_and_join_threads() {
    {
      std::lock_guard lock(queue_mutex);
      stop = true;
    }
    condition_variable.notify_all();
    for (auto &thread : threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  std::vector<Scope<CommandBuffer>>
      command_buffers; // Pre-allocated command buffers
  std::vector<std::thread> threads;
  std::queue<std::function<void(usize)>>
      tasks; // Tasks now take a command buffer index
  std::mutex queue_mutex;
  std::mutex other_mutex;
  std::condition_variable condition_variable;
  bool stop{false};
  const Device *device;
};

} // namespace Core
