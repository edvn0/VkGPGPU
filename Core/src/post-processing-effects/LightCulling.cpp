#include "SceneRenderer.hpp"

namespace Core {

auto SceneRenderer::light_culling_pass() -> void {
  if (spot_light_ubo.count == 0 || point_light_ubo.count == 0) {
    return;
  }

  compute_command_buffer->begin(current_frame);
  gpu_time_queries.light_culling_pass_query =
      compute_command_buffer->begin_timestamp_query();
  light_culling_material->set("shadow_map", get_depth_image());
  light_culling_pipeline->bind(*compute_command_buffer);

  update_material_for_rendering(current_frame, *light_culling_material,
                                ubos.get(), ssbos.get());
  light_culling_material->bind(*compute_command_buffer, *light_culling_pipeline,
                               current_frame);

  push_constants(*light_culling_pipeline, *light_culling_material);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(),
                light_culling_workgroup_size.x, light_culling_workgroup_size.y,
                1);

  compute_command_buffer->end_timestamp_query(
      gpu_time_queries.light_culling_pass_query);
  compute_command_buffer->end_and_submit();
}

} // namespace Core
