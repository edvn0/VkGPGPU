#pragma once

#include "CommandBuffer.hpp"

namespace Core {

class CommandDispatcher {
  struct GroupSize {
    u32 group_count_x{1};
    u32 group_count_y{1};
    u32 group_count_z{1};
  };

public:
  CommandDispatcher(CommandBuffer *command_buffer)
      : command_buffer(command_buffer) {}

  // Non-owning destructor
  ~CommandDispatcher() = default;

  template <CommandBufferBindable T, typename... Args>
  void bind(T &object, Args &&...args) {
    object.bind(*command_buffer, std::forward<Args>(args)...);
  }

  auto dispatch(const GroupSize &group_size) -> void {
    vkCmdDispatch(command_buffer->get_command_buffer(),
                  group_size.group_count_x, group_size.group_count_y,
                  group_size.group_count_z);
  }

private:
  CommandBuffer *command_buffer{nullptr};
};

} // namespace Core