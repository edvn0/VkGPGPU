#pragma once

#include "CommandBuffer.hpp"
#include "Material.hpp"
#include "Pipeline.hpp"

namespace Core {

class CommandDispatcher {
  struct GroupSize {
    u32 group_count_x{1};
    u32 group_count_y{1};
    u32 group_count_z{1};
  };

public:
  explicit CommandDispatcher(const CommandBuffer *command_buffer)
      : command_buffer(command_buffer) {}

  // Non-owning destructor
  ~CommandDispatcher() = default;

  auto set_command_buffer(const CommandBuffer *new_command_buffer) -> void {
    command_buffer = new_command_buffer;
  }

  template <CommandBufferBindable T, typename... Args>
  void bind(T &object, Args &&...args) {
    object.bind(*command_buffer, std::forward<Args>(args)...);
  }

  auto push_constant(const ComputePipeline &pipeline,
                     const Material &material) {
    const auto &constant_buffer = material.get_constant_buffer();

    if (!constant_buffer.valid())
      return;

    vkCmdPushConstants(command_buffer->get_command_buffer(),
                       pipeline.get_pipeline_layout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, constant_buffer.size(),
                       constant_buffer.raw());
  }

  auto dispatch(const GroupSize &group_size) const -> void {
    vkCmdDispatch(command_buffer->get_command_buffer(),
                  group_size.group_count_x, group_size.group_count_y,
                  group_size.group_count_z);
  }

private:
  const CommandBuffer *command_buffer{nullptr};
};

} // namespace Core
