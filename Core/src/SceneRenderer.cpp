#include "pch/vkgpgpu_pch.hpp"

#include "SceneRenderer.hpp"

#include "CommandDispatcher.hpp"
#include "Input.hpp"
#include "Random.hpp"

#include "compilation/ShaderCompiler.hpp"

namespace Core {

auto SceneRenderer::begin_gpu_debug_frame_marker(
    const CommandBuffer &, std::string_view current_frame_identifier) -> void {
  debug_marker_stack.emplace(current_frame_identifier);
}

auto SceneRenderer::end_gpu_debug_frame_marker(
    const CommandBuffer &, std::string_view current_frame_identifier) -> void {
  ensure(!debug_marker_stack.empty(),
         "Stack is empty, invalid call to end_gpu_debug_frame_marker. Missing "
         "a begin_gpu_debug_frame_marker call?");
  ensure(debug_marker_stack.top() == std::string{current_frame_identifier},
         "The 'begin_gpu_debug_frame_marker' and 'end_gpu_debug_frame_marker' "
         "calls do not match. Ensure that the same string is passed to both "
         "functions.");

  debug_marker_stack.pop();
}

template <Core::Buffer::Type T>
auto create_or_get_write_descriptor_for(u32 frames_in_flight,
                                        Core::BufferSet<T> *buffer_set,
                                        const Material &material)
    -> const std::vector<std::vector<VkWriteDescriptorSet>> & {
  static std::unordered_map<
      BufferSet<T> *,
      std::unordered_map<std::size_t,
                         std::vector<std::vector<VkWriteDescriptorSet>>>>
      buffer_set_write_descriptor_cache;

  static constexpr const auto get_descriptor_set_vector =
      [](auto &map, auto *key, auto hash) -> auto & { return map[key][hash]; };

  const auto &vulkan_shader = material.get_shader();
  const auto shader_hash = vulkan_shader.hash();
  if (buffer_set_write_descriptor_cache.contains(buffer_set)) {
    const auto &shader_map = buffer_set_write_descriptor_cache.at(buffer_set);
    if (shader_map.contains(shader_hash)) {
      const auto &write_descriptors = shader_map.at(shader_hash);
      return write_descriptors;
    }
  }

  if (!vulkan_shader.has_descriptor_set(0)) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_sets =
      vulkan_shader.get_reflection_data().shader_descriptor_sets;
  if (shader_descriptor_sets.empty()) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_set = shader_descriptor_sets[0];
  const auto &storage_buffers = shader_descriptor_set.storage_buffers;
  const auto &uniform_buffers = shader_descriptor_set.uniform_buffers;

  if constexpr (T == Buffer::Type::Storage) {
    for (const auto &[k, v] : storage_buffers) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        auto *maybe_buffer =
            buffer_set->try_get(DescriptorBinding(k), frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        if (maybe_buffer != nullptr) {
          auto &stored_buffer = *maybe_buffer;
          auto &buffer_info = stored_buffer.get_descriptor_info();
          auto binding = stored_buffer.get_binding();
          wds.pBufferInfo = &buffer_info;
          wds.dstBinding = binding;
        } else {
          auto &stored_buffer = material.get_stored_buffer(k);
          auto &buffer_info = stored_buffer->get_descriptor_info();
          auto binding = stored_buffer->get_binding();
          wds.pBufferInfo = &buffer_info;
          wds.dstBinding = binding;
        }

        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_descriptors[frame].push_back(wds);
      }
    }
  } else {
    for (const auto &binding : uniform_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
        write_descriptors[frame].push_back(wds);
      }
    }
  }

  return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                   buffer_set, shader_hash);
}

auto SceneRenderer::create_preetham_sky(float turbidity, float azimuth,
                                        float inclination) -> Ref<TextureCube> {
  const u32 cubemap_size = Config::environment_map_size;

  const Extent<u32> cubemap_extent = {
      cubemap_size,
      cubemap_size,
  };
  auto environment_map =
      TextureCube::construct(*device, ImageFormat::SRGB_RGBA32, cubemap_extent);

  auto preetham_sky_shader = shader_cache.at("PreethamSky").get();
  auto preetham_sky_compute_pipeline =
      ComputePipeline::construct(*device, ComputePipelineConfiguration{
                                              "PreethamSkyComputePipeline",
                                              PipelineStage::Compute,
                                              *preetham_sky_shader,
                                          });

  glm::vec3 params = {turbidity, azimuth, inclination};

  std::array<VkWriteDescriptorSet, 1> write_descriptors{};
  auto descriptor_set = preetham_sky_shader->allocate_descriptor_set(0);
  write_descriptors[0] =
      *preetham_sky_shader->get_descriptor_set("output_cubemap", 0);
  write_descriptors[0].dstSet = descriptor_set.descriptor_sets[0];
  write_descriptors[0].pImageInfo = &environment_map->get_descriptor_info();

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<u32>(write_descriptors.size()),
                         write_descriptors.data(), 0, nullptr);

  compute_command_buffer->begin(current_frame);
  vkCmdBindPipeline(compute_command_buffer->get_command_buffer(),
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    preetham_sky_compute_pipeline->get_pipeline());
  vkCmdPushConstants(compute_command_buffer->get_command_buffer(),
                     preetham_sky_compute_pipeline->get_pipeline_layout(),
                     VK_SHADER_STAGE_ALL, 0, sizeof(glm::vec3), &params);
  vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          preetham_sky_compute_pipeline->get_pipeline_layout(),
                          0, 1, &descriptor_set.descriptor_sets[0], 0, nullptr);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(), cubemap_size / 32,
                cubemap_size / 32, 6);
  compute_command_buffer->end_and_submit();

  environment_map->generate_mips(true);

  return environment_map;
}

auto SceneRenderer::on_resize(const Extent<u32> &new_extent) -> void {
  extent = new_extent;
  inverse_extent = {
      .width = 1.0F / static_cast<float>(extent.width),
      .height = 1.0F / static_cast<float>(extent.height),
  };
  geometry_framebuffer->on_resize(extent);
  shadow_framebuffer->on_resize(extent);
  fullscreen_framebuffer->on_resize(extent);

  skybox_material->on_resize(extent);
  shadow_material->on_resize(extent);
  grid_material->on_resize(extent);
  fullscreen_material->on_resize(extent);

  for (auto &[key, shader] : shader_cache) {
    shader->on_resize(extent);
  }
}

auto SceneRenderer::destroy() -> void {
  Destructors::destroy(device, pool);
  Destructors::destroy(device, layout);

  white_texture.reset();
  black_texture.reset();
  brdf_lookup_texture.reset();
}

auto SceneRenderer::begin_renderpass(const Framebuffer &framebuffer) -> void {
  VkRenderPassBeginInfo render_pass_begin_info = {};
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.renderPass = framebuffer.get_render_pass();
  render_pass_begin_info.framebuffer = framebuffer.get_framebuffer();
  render_pass_begin_info.renderArea.extent = {
      .width = framebuffer.get_width(),
      .height = framebuffer.get_height(),
  };
  const auto &clear_values = framebuffer.get_clear_values();
  render_pass_begin_info.clearValueCount =
      static_cast<u32>(clear_values.size());
  render_pass_begin_info.pClearValues = clear_values.data();
  vkCmdBeginRenderPass(command_buffer->get_command_buffer(),
                       &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

  // Scissors and viewport
  VkViewport viewport = {};
  viewport.x = 0.0F;
  viewport.y = static_cast<float>(framebuffer.get_height());
  viewport.width = static_cast<float>(framebuffer.get_width());
  viewport.height = -static_cast<float>(framebuffer.get_height());
  viewport.minDepth = 1.0F;
  viewport.maxDepth = 0.0F;

  if (!framebuffer.get_properties().flip_viewport) {
    viewport.y = 0.0F;
    viewport.height = static_cast<float>(framebuffer.get_height());
  }

  vkCmdSetViewport(command_buffer->get_command_buffer(), 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.extent.width = framebuffer.get_width();
  scissor.extent.height = framebuffer.get_height();
  vkCmdSetScissor(command_buffer->get_command_buffer(), 0, 1, &scissor);
}

auto SceneRenderer::explicit_clear(const Framebuffer &framebuffer) -> void {
  const std::vector<VkClearValue> &fb_clear_values =
      framebuffer.get_clear_values();

  const auto color_attachment_count = framebuffer.get_colour_attachment_count();
  const auto total_attachment_count =
      color_attachment_count + (framebuffer.has_depth_attachment() ? 1 : 0);

  const VkExtent2D extent_2_d = {
      .width = framebuffer.get_width(),
      .height = framebuffer.get_height(),
  };

  std::vector<VkClearAttachment> attachments(total_attachment_count);
  std::vector<VkClearRect> clear_rects(total_attachment_count);
  for (u32 i = 0; i < color_attachment_count; i++) {
    attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachments[i].colorAttachment = i;
    attachments[i].clearValue = fb_clear_values[i];

    clear_rects[i].rect.offset = {
        0,
        0,
    };
    clear_rects[i].rect.extent = extent_2_d;
    clear_rects[i].baseArrayLayer = 0;
    clear_rects[i].layerCount = 1;
  }

  if (framebuffer.has_depth_attachment()) {
    auto aspect_bits = framebuffer.get_depth_image()->get_aspect_bits();
    attachments[color_attachment_count].aspectMask = aspect_bits;
    attachments[color_attachment_count].clearValue =
        fb_clear_values[color_attachment_count];
    clear_rects[color_attachment_count].rect.offset = {0, 0};
    clear_rects[color_attachment_count].rect.extent = extent_2_d;
    clear_rects[color_attachment_count].baseArrayLayer = 0;
    clear_rects[color_attachment_count].layerCount = 1;
  }

  vkCmdClearAttachments(
      command_buffer->get_command_buffer(),
      static_cast<u32>(total_attachment_count), attachments.data(),
      static_cast<u32>(total_attachment_count), clear_rects.data());
}

auto SceneRenderer::draw(const DrawParameters &params) const -> void {
  vkCmdDrawIndexed(command_buffer->get_command_buffer(), params.index_count,
                   params.instance_count, params.first_index,
                   static_cast<i32>(params.vertex_offset),
                   params.first_instance);
}

auto SceneRenderer::draw_vertices(const DrawParameters &params) const -> void {
  vkCmdDraw(command_buffer->get_command_buffer(), params.vertex_count,
            params.instance_count, params.first_index, params.first_instance);
}

auto SceneRenderer::bind_pipeline(const GraphicsPipeline &pipeline) -> void {
  if (bound_pipeline.hash == pipeline.hash()) {
    return;
  }

  pipeline.bind(*command_buffer);
  bound_pipeline = {
      .bound_pipeline = pipeline.get_pipeline(),
      .hash = pipeline.hash(),
  };
}

auto SceneRenderer::bind_index_buffer(const Buffer &index_buffer) const
    -> void {
  vkCmdBindIndexBuffer(command_buffer->get_command_buffer(),
                       index_buffer.get_buffer(), 0, VK_INDEX_TYPE_UINT32);
}

auto SceneRenderer::bind_vertex_buffer(const Buffer &vertex_buffer) const
    -> void {
  bind_vertex_buffer(vertex_buffer, 0, 0);
}

auto SceneRenderer::bind_vertex_buffer(const Buffer &vertex_buffer,
                                       u32 vertex_offset, u32 index) const
    -> void {
  const std::array<VkDeviceSize, 1> offset = {vertex_offset};
  const std::array buffers = {vertex_buffer.get_buffer()};
  vkCmdBindVertexBuffers(command_buffer->get_command_buffer(), index, 1,
                         buffers.data(), offset.data());
}

auto SceneRenderer::submit_aabb(const AABB &aabb, const glm::mat4 &transform)
    -> void {
  geometry_renderer.submit_aabb(aabb, transform);
}

auto SceneRenderer::submit_cube(const glm::mat4 &transform,
                                const glm::vec4 &colour) -> void {
  const auto &mesh = Mesh::get_cube();
  submit_static_mesh(mesh.get(), transform, colour);
}

auto SceneRenderer::submit_frustum(const glm::mat4 &inverse_view_projection,
                                   const glm::mat4 &transform) -> void {
  geometry_renderer.submit_frustum(inverse_view_projection, transform);
}

auto SceneRenderer::submit_static_mesh(const Mesh *mesh,
                                       const glm::mat4 &transform,
                                       const glm::vec4 &colour) -> void {
  for (const auto &submesh : mesh->get_submeshes()) {
    CommandKey key{mesh, submesh};

    auto &mesh_transform = mesh_transform_map[key].transforms.emplace_back();
    mesh_transform.transform_rows[0] = {transform[0][0], transform[1][0],
                                        transform[2][0], transform[3][0]};
    mesh_transform.transform_rows[1] = {transform[0][1], transform[1][1],
                                        transform[2][1], transform[3][1]};
    mesh_transform.transform_rows[2] = {transform[0][2], transform[1][2],
                                        transform[2][2], transform[3][2]};
    // We also instance the colour :)
    mesh_transform.transform_rows[3] = colour;

    auto &command = draw_commands[key];
    command.mesh_ptr = mesh;
    command.submesh_index = submesh;
    command.instance_count++;
    command.material = mesh->get_material(submesh);

    if (mesh->casts_shadows()) {
      auto &shadow_command = shadow_draw_commands[key];
      shadow_command.mesh_ptr = mesh;
      shadow_command.submesh_index = submesh;
      shadow_command.instance_count++;
      shadow_command.material = shadow_material.get();
    }
  }
}

auto SceneRenderer::end_renderpass() -> void {
  vkCmdEndRenderPass(command_buffer->get_command_buffer());
}

auto SceneRenderer::begin_frame(const glm::mat4 &, const glm::mat4 &) -> void {
  // Bloom
  {
    glm::uvec2 bloom_size = (glm::uvec2{extent.width, extent.height} + 1u) / 2u;
    bloom_size += bloom_workgroup_size - bloom_size % bloom_workgroup_size;
    bloom_textures[0]->on_resize({bloom_size.x, bloom_size.y});
    bloom_textures[1]->on_resize({bloom_size.x, bloom_size.y});
    bloom_textures[2]->on_resize({bloom_size.x, bloom_size.y});
  }

  {
    static constexpr uint32_t tiling_size = 16u;
    glm::uvec2 size = glm::uvec2{extent.width, extent.height};
    size += tiling_size - size % tiling_size;
    light_culling_workgroup_size = {size / tiling_size, 1};
    renderer_ubo.tiles_count = {
        light_culling_workgroup_size.x,
        light_culling_workgroup_size.y,
    };

    auto &visible_point_lights_ssbo = ssbos->get(7, current_frame);
    auto &visible_spot_lights_ssbo = ssbos->get(8, current_frame);

    visible_point_lights_ssbo->resize(light_culling_workgroup_size.x *
                                      light_culling_workgroup_size.y * 4 *
                                      1024);
    visible_spot_lights_ssbo->resize(light_culling_workgroup_size.x *
                                     light_culling_workgroup_size.y * 4 * 1024);
  }

  const auto &ubo = ubos->get(0, current_frame);
  ubo->write(renderer_ubo);

  const auto &ubo_shadow = ubos->get(1, current_frame);
  shadow_ubo.bias_and_default = {depth_factor.bias, depth_factor.default_value};

  ubo_shadow->write(shadow_ubo);

  const auto &ubo_grid = ubos->get(3, current_frame);
  ubo_grid->write(grid_ubo);

  vkResetDescriptorPool(device->get_device(), pool, 0);

  VkDescriptorSetAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorPool = pool;
  allocation_info.descriptorSetCount = 1;
  allocation_info.pSetLayouts = &layout;
  vkAllocateDescriptorSets(device->get_device(), &allocation_info, &active);

  const auto ubo_write_descriptors = ubos->get_write_descriptors(current_frame);
  const auto sbo_write_descriptors =
      ssbos->get_write_descriptors(current_frame);
  auto combined =
      Container::combine_into_one(ubo_write_descriptors, sbo_write_descriptors);

  Container::for_each(combined,
                      [s = active](auto &write) { write.dstSet = s; });

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<u32>(combined.size()), combined.data(), 0,
                         nullptr);
}

auto SceneRenderer::shadow_pass() -> void {
  bind_pipeline(*shadow_pipeline);
  for (const auto &[key, value] : shadow_draw_commands) {
    auto &&[mesh_ptr, submesh_index, instance_count, material] = value;

    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    update_material_for_rendering(current_frame, *shadow_material, ubos.get(),
                                  ssbos.get());
    shadow_material->bind(*command_buffer, *shadow_pipeline, current_frame);

    const auto &mesh_vertex_buffer = *mesh_ptr->get_vertex_buffer();
    const auto &transform_vertex_buffer =
        *transform_buffers[current_frame].vertex_buffer;

    bind_vertex_buffer(mesh_vertex_buffer);
    bind_vertex_buffer(transform_vertex_buffer,
                       mesh_transform_map.at(key).offset, 1);
    bind_index_buffer(*mesh_ptr->get_index_buffer());

    draw({
        .index_count = submesh.index_count,
        .instance_count = instance_count,
        .first_index = submesh.base_index,
        .vertex_offset = submesh.base_vertex,
    });
  }

  geometry_renderer.flush(*command_buffer, current_frame, shadow_pipeline.get(),
                          shadow_material.get());
}

auto SceneRenderer::light_culling_pass() -> void {

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

auto SceneRenderer::geometry_pass() -> void {
  static bool wireframe = false;
  if (Input::pressed(KeyCode::KEY_F3)) {
    wireframe = !wireframe;
  }

  const auto *selected_pipeline =
      wireframe ? wireframed_geometry_pipeline.get() : geometry_pipeline.get();

  bind_pipeline(*selected_pipeline);
  for (auto &&[key, value] : draw_commands) {
    auto &&[mesh_ptr, submesh_index, instance_count, material] = value;
    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    if (material) {
      material->set("shadow_map", *shadow_framebuffer->get_depth_image());
      material->set("irradiance_texture",
                    *scene_environment.irradiance_texture);
      material->set("radiance_texture", *scene_environment.radiance_texture);
      material->set("brdf_lookup", SceneRenderer::get_brdf_lookup_texture());
      update_material_for_rendering(current_frame, *material, ubos.get(),
                                    ssbos.get());
      material->bind(*command_buffer, *selected_pipeline, current_frame);
      push_constants(*selected_pipeline, *material);
    }

    const auto &mesh_vertex_buffer = *mesh_ptr->get_vertex_buffer();
    const auto &transform_vertex_buffer =
        *transform_buffers[current_frame].vertex_buffer;

    bind_vertex_buffer(mesh_vertex_buffer);
    bind_vertex_buffer(transform_vertex_buffer,
                       mesh_transform_map.at(key).offset, 1);
    bind_index_buffer(*mesh_ptr->get_index_buffer());

    if (wireframe) {
      vkCmdSetLineWidth(command_buffer->get_command_buffer(), 2.0F);
    }

    draw({
        .index_count = submesh.index_count,
        .instance_count = instance_count,
        .first_index = submesh.base_index,
        .vertex_offset = submesh.base_vertex,
    });
  }

  geometry_renderer.update_all_materials_for_rendering(*this);
  geometry_renderer.flush(*command_buffer, current_frame);
}

void SceneRenderer::bloom_pass() {
  static auto first_iteration = true;
  if (first_iteration) {
    first_iteration = false;
    return;
  }

  if (!bloom_settings.enabled)
    return;

  enum class BloomMode : int {
    Prefilter,
    Downsample,
    FirstUpsample,
    Upsample,
  };

  struct {
    glm::vec4 params;
    float LOD = 0.0f;
    // 0 = prefilter, 1 = downsample, 2 = firstUpsample, 3 = upsample
    BloomMode mode = BloomMode::Prefilter;
  } bloom_compute_push_constants;

  bloom_compute_push_constants.params = {
      bloom_settings.threshold, bloom_settings.threshold - bloom_settings.knee,
      bloom_settings.knee * 2.0f, 0.25f / bloom_settings.knee};
  bloom_compute_push_constants.mode = BloomMode::Prefilter;

  const auto &input_image = geometry_framebuffer->get_image();

  std::array images = {
      &bloom_textures.at(0)->get_image(),
      &bloom_textures.at(1)->get_image(),
      &bloom_textures.at(2)->get_image(),
  };

  const auto &shader = bloom_material->get_shader();

  auto descriptor_image_info = images[0]->get_descriptor_info();
  descriptor_image_info.imageView = images[0]->get_mip_image_view_at(0);

  std::array<VkWriteDescriptorSet, 3> write_descriptors{};

  VkDescriptorSetLayout descriptorSetLayout =
      shader.get_descriptor_set_layouts().at(0);

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorSetLayout;

  compute_command_buffer->begin(current_frame);
  gpu_time_queries.bloom_compute_pass_query =
      compute_command_buffer->begin_timestamp_query();

  // Output image
  VkDescriptorSet descriptor_set =
      device->get_descriptor_resource()->allocate_descriptor_set(allocInfo);
  write_descriptors[0] =
      *shader.get_descriptor_set("bloom_output_storage_image", 0);
  write_descriptors[0].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[0].pImageInfo = &descriptor_image_info;

  // Input image
  write_descriptors[1] =
      *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
  write_descriptors[1].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[1].pImageInfo = &input_image->get_descriptor_info();

  write_descriptors[2] = *shader.get_descriptor_set("bloom_output_texture", 0);
  write_descriptors[2].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

  vkUpdateDescriptorSets(device->get_device(), (u32)write_descriptors.size(),
                         write_descriptors.data(), 0, nullptr);

  u32 workgroups_x =
      bloom_textures[0]->get_extent().width / bloom_workgroup_size;
  u32 workgroups_y =
      bloom_textures[0]->get_extent().height / bloom_workgroup_size;

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-Prefilter");
  bloom_pipeline->bind(*compute_command_buffer);
  vkCmdPushConstants(compute_command_buffer->get_command_buffer(),
                     bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL,
                     0, sizeof(bloom_compute_push_constants),
                     &bloom_compute_push_constants);
  vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                          bloom_pipeline->get_bind_point(),
                          bloom_pipeline->get_pipeline_layout(), 0, 1,
                          &descriptor_set, 0, nullptr);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                workgroups_y, 1);
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-Prefilter");

  {
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = images[0]->get_image();
    imageMemoryBarrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        images[0]->get_properties().mip_info.mips,
        0,
        1,
    };
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);
  }

  bloom_compute_push_constants.mode = BloomMode::Downsample;

  u32 offset = 2;
  u32 mips =
      bloom_textures[0]->get_image().get_properties().mip_info.mips - offset;
  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-DownSample");
  for (u32 i = 1; i < mips; i++) {
    auto [mip_width, mip_height] = bloom_textures[0]->get_mip_size(i);
    workgroups_x =
        (u32)glm::ceil((float)mip_width / (float)bloom_workgroup_size);
    workgroups_y =
        (u32)glm::ceil((float)mip_height / (float)bloom_workgroup_size);

    {
      // Output image
      descriptor_image_info.imageView = images[1]->get_mip_image_view_at(i);

      descriptor_set =
          device->get_descriptor_resource()->allocate_descriptor_set(allocInfo);
      write_descriptors[0] =
          *shader.get_descriptor_set("bloom_output_storage_image", 0);
      write_descriptors[0].dstSet =
          descriptor_set; // Should this be set inside the shader?
      write_descriptors[0].pImageInfo = &descriptor_image_info;

      // Input image
      write_descriptors[1] =
          *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
      write_descriptors[1].dstSet =
          descriptor_set; // Should this be set inside the shader?
      auto descriptor = bloom_textures[0]->get_image().get_descriptor_info();
      // descriptor.sampler = samplerClamp;
      write_descriptors[1].pImageInfo = &descriptor;

      write_descriptors[2] =
          *shader.get_descriptor_set("bloom_output_texture", 0);
      write_descriptors[2].dstSet =
          descriptor_set; // Should this be set inside the shader?
      write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

      vkUpdateDescriptorSets(device->get_device(),
                             (u32)write_descriptors.size(),
                             write_descriptors.data(), 0, nullptr);

      bloom_compute_push_constants.LOD = static_cast<float>(i) - 1.0f;
      vkCmdPushConstants(
          compute_command_buffer->get_command_buffer(),
          bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
          sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
      vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                              bloom_pipeline->get_bind_point(),
                              bloom_pipeline->get_pipeline_layout(), 0, 1,
                              &descriptor_set, 0, nullptr);
      vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                    workgroups_y, 1);
    }

    {
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.image = images[1]->get_image();
      imageMemoryBarrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          images[1]->get_properties().mip_info.mips,
          0,
          1,
      };
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &imageMemoryBarrier);
    }

    {
      descriptor_image_info.imageView = images[0]->get_mip_image_view_at(i);

      // Output image
      descriptor_set =
          device->get_descriptor_resource()->allocate_descriptor_set(allocInfo);
      write_descriptors[0] =
          *shader.get_descriptor_set("bloom_output_storage_image", 0);
      write_descriptors[0].dstSet =
          descriptor_set; // Should this be set inside the shader?
      write_descriptors[0].pImageInfo = &descriptor_image_info;

      // Input image
      write_descriptors[1] =
          *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
      write_descriptors[1].dstSet =
          descriptor_set; // Should this be set inside the shader?
      auto descriptor = bloom_textures[1]->get_image().get_descriptor_info();
      // descriptor.sampler = samplerClamp;
      write_descriptors[1].pImageInfo = &descriptor;

      write_descriptors[2] =
          *shader.get_descriptor_set("bloom_output_texture", 0);
      write_descriptors[2].dstSet =
          descriptor_set; // Should this be set inside the shader?
      write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

      vkUpdateDescriptorSets(device->get_device(),
                             (u32)write_descriptors.size(),
                             write_descriptors.data(), 0, nullptr);

      bloom_compute_push_constants.LOD = (float)i;
      vkCmdPushConstants(
          compute_command_buffer->get_command_buffer(),
          bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
          sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
      vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                              bloom_pipeline->get_bind_point(),
                              bloom_pipeline->get_pipeline_layout(), 0, 1,
                              &descriptor_set, 0, nullptr);
      vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                    workgroups_y, 1);
    }

    {
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.image = images[0]->get_image();
      imageMemoryBarrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0,
          images[0]->get_properties().mip_info.mips, 0, 1};
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &imageMemoryBarrier);
    }
  }
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-DownSample");

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-FirstUpsample");
  bloom_compute_push_constants.mode = BloomMode::FirstUpsample;
  workgroups_x *= 2;
  workgroups_y *= 2;

  // Output image
  descriptor_set =
      device->get_descriptor_resource()->allocate_descriptor_set(allocInfo);
  descriptor_image_info.imageView = images[2]->get_mip_image_view_at(mips - 2);

  write_descriptors[0] =
      *shader.get_descriptor_set("bloom_output_storage_image", 0);
  write_descriptors[0].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[0].pImageInfo = &descriptor_image_info;

  // Input image
  write_descriptors[1] =
      *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
  write_descriptors[1].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[1].pImageInfo =
      &bloom_textures[0]->get_image().get_descriptor_info();

  write_descriptors[2] = *shader.get_descriptor_set("bloom_output_texture", 0);
  write_descriptors[2].dstSet =
      descriptor_set; // Should this be set inside the shader?
  write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

  vkUpdateDescriptorSets(device->get_device(), (u32)write_descriptors.size(),
                         write_descriptors.data(), 0, nullptr);

  bloom_compute_push_constants.LOD--;
  vkCmdPushConstants(compute_command_buffer->get_command_buffer(),
                     bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL,
                     0, sizeof(bloom_compute_push_constants),
                     &bloom_compute_push_constants);

  auto &&[mip_width, mip_height] = bloom_textures[2]->get_mip_size(mips - 2);
  workgroups_x = (u32)glm::ceil((float)mip_width / (float)bloom_workgroup_size);
  workgroups_y =
      (u32)glm::ceil((float)mip_height / (float)bloom_workgroup_size);
  vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                          bloom_pipeline->get_bind_point(),
                          bloom_pipeline->get_pipeline_layout(), 0, 1,
                          &descriptor_set, 0, nullptr);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                workgroups_y, 1);
  {
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = images[2]->get_image();
    imageMemoryBarrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, images[2]->get_properties().mip_info.mips,
        0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);
  }

  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-FirstUpsample");

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-Upsample");
  bloom_compute_push_constants.mode = BloomMode::Upsample;

  // Upsample
  for (i32 mip = static_cast<i32>(mips) - 3; mip >= 0; mip--) {
    auto [texture_mip_width, texture_mip_height] =
        bloom_textures[2]->get_mip_size(mip);
    workgroups_x =
        (u32)glm::ceil((float)texture_mip_width / (float)bloom_workgroup_size);
    workgroups_y =
        (u32)glm::ceil((float)texture_mip_height / (float)bloom_workgroup_size);

    // Output image
    descriptor_image_info.imageView = images[2]->get_mip_image_view_at(mip);
    auto descriptorSet =
        device->get_descriptor_resource()->allocate_descriptor_set(allocInfo);
    write_descriptors[0] =
        *shader.get_descriptor_set("bloom_output_storage_image", 0);
    write_descriptors[0].dstSet =
        descriptorSet; // Should this be set inside the shader?
    write_descriptors[0].pImageInfo = &descriptor_image_info;

    // Input image
    write_descriptors[1] =
        *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
    write_descriptors[1].dstSet =
        descriptorSet; // Should this be set inside the shader?
    write_descriptors[1].pImageInfo =
        &bloom_textures[0]->get_image().get_descriptor_info();

    write_descriptors[2] =
        *shader.get_descriptor_set("bloom_output_texture", 0);
    write_descriptors[2].dstSet =
        descriptorSet; // Should this be set inside the shader?
    write_descriptors[2].pImageInfo = &images[2]->get_descriptor_info();

    vkUpdateDescriptorSets(device->get_device(), (u32)write_descriptors.size(),
                           write_descriptors.data(), 0, nullptr);

    bloom_compute_push_constants.LOD = (float)mip;
    vkCmdPushConstants(
        compute_command_buffer->get_command_buffer(),
        bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
        sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
    vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                            bloom_pipeline->get_bind_point(),
                            bloom_pipeline->get_pipeline_layout(), 0, 1,
                            &descriptorSet, 0, nullptr);
    vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                  workgroups_y, 1);

    {
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.image = images[2]->get_image();
      imageMemoryBarrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0,
          images[2]->get_properties().mip_info.mips, 0, 1};
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &imageMemoryBarrier);
    }
  }
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-Upsample");

  compute_command_buffer->end_timestamp_query(
      gpu_time_queries.bloom_compute_pass_query);
  compute_command_buffer->end_and_submit();
}

auto SceneRenderer::debug_pass() -> void {
  // AABBs with cube_mesh, from debug_draw_commands
}

auto SceneRenderer::environment_pass() -> void {
  skybox_material->set("texture_cube", *scene_environment.radiance_texture);

  bind_pipeline(*skybox_pipeline);
  update_material_for_rendering(current_frame, *skybox_material, ubos.get(),
                                nullptr);
  skybox_material->bind(*command_buffer, *skybox_pipeline, current_frame);
  push_constants(*skybox_pipeline, *skybox_material);

  draw_vertices({
      .vertex_count = 6,
      .instance_count = 1,
  });
}

auto SceneRenderer::grid_pass() -> void {
  bind_pipeline(*grid_pipeline);
  update_material_for_rendering(current_frame, *grid_material, ubos.get(),
                                ssbos.get());
  grid_material->bind(*command_buffer, *grid_pipeline, current_frame);
  push_constants(*grid_pipeline, *grid_material);

  draw_vertices({
      .vertex_count = 6,
      .instance_count = 1,
  });
}

auto SceneRenderer::fullscreen_pass() -> void {
  bind_pipeline(*fullscreen_pipeline);

  float Exposure = 0.8F;
  float BloomIntensity = bloom_settings.intensity;
  float BloomDirtIntensity = bloom_settings.dirt_intensity;
  float Opacity = 1.0F;

  fullscreen_material->set("bloom_geometry_input_texture",
                           *geometry_framebuffer->get_image(0));
  fullscreen_material->set("u_DepthTexture", get_depth_image());

  if (!bloom_settings.enabled) {
    BloomIntensity = 0.0f;
    BloomDirtIntensity = 0.0f;
    fullscreen_material->set("bloom_output_texture",
                             SceneRenderer::get_white_texture());
  }
  fullscreen_material->set("bloom_output_texture", *bloom_textures[2]);
  fullscreen_material->set("u_BloomDirtTexture",
                           SceneRenderer::get_black_texture());

  const auto data =
      glm::vec4{Exposure, BloomIntensity, BloomDirtIntensity, Opacity};
  fullscreen_material->set("bloom_uniforms.values", data);

  push_constants(*fullscreen_pipeline, *fullscreen_material);

  update_material_for_rendering(current_frame, *fullscreen_material, ubos.get(),
                                ssbos.get());
  fullscreen_material->bind(*command_buffer, *fullscreen_pipeline,
                            current_frame);
  draw_vertices({
      .vertex_count = 3,
  });
}

auto SceneRenderer::begin_scene(const ECS::Scene &scene, FrameIndex frame_index)
    -> void {
  set_frame_index(frame_index);

  scene_data.light_environment = scene.get_light_environment();

  const auto &lightEnvironment = scene_data.light_environment;
  const std::vector<ECS::PointLight> &pointLightsVec =
      lightEnvironment.point_light_buffer;
  point_light_ubo.count = int(pointLightsVec.size());
  std::memcpy(point_light_ubo.lights.data(), pointLightsVec.data(),
              lightEnvironment.get_point_light_buffer_size_bytes());

  auto &point_light_ubo_write = ubos->get(4, frame_index);
  point_light_ubo_write->write(&point_light_ubo,
                               16ull + sizeof(ECS::PointLight) *
                                           point_light_ubo.count);

  const std::vector<ECS::SpotLight> &spotLightsVec =
      lightEnvironment.spot_light_buffer;
  spot_light_ubo.count = int(spotLightsVec.size());
  std::memcpy(spot_light_ubo.lights.data(), spotLightsVec.data(),
              lightEnvironment.get_spot_light_buffer_size_bytes());
  auto &spot_light_ubo_write = ubos->get(5, frame_index);
  spot_light_ubo_write->write(
      &spot_light_ubo, 16ull + sizeof(ECS::SpotLight) * spot_light_ubo.count);

  for (size_t i = 0; i < spotLightsVec.size(); ++i) {
    auto &light = spotLightsVec[i];
    if (!light.casts_shadows)
      continue;

    glm::mat4 projection =
        glm::perspective(glm::radians(light.angle), 1.f, 0.1f, light.range);
    spot_shadows_ubo.shadow_matrices[i] =
        projection * glm::lookAt(light.position,
                                 light.position - light.direction,
                                 glm::vec3(0.0f, 1.0f, 0.0f));
  }

  auto &spot_light_matrix_data = ubos->get(6, frame_index);
  spot_light_matrix_data->write(
      &spot_shadows_ubo,
      static_cast<u32>(sizeof(glm::mat4) * spotLightsVec.size()));

  const glm::uvec2 viewportSize = {extent.width, extent.height};

  screen_data_ubo.full_resolution = {
      viewportSize.x,
      viewportSize.y,
  };
  screen_data_ubo.inverse_full_resolution = {
      inverse_extent.width,
      inverse_extent.height,
  };
  screen_data_ubo.half_resolution =
      glm::ivec2{
          viewportSize.x,
          viewportSize.y,
      } /
      2;
  screen_data_ubo.inverse_half_resolution = {
      2.0F * inverse_extent.width,
      2.0F * inverse_extent.height,
  };

  ubos->get(9, frame_index)->write(screen_data_ubo);
}

auto SceneRenderer::flush() -> void {
  u32 offset = 0;
  auto &&[vb, tb] = transform_buffers.at(current_frame);

  for (auto &transform_data : mesh_transform_map | std::views::values) {
    transform_data.offset = offset * sizeof(TransformVertexData);
    for (const auto &transform : transform_data.transforms) {
      tb->write(&transform, sizeof(TransformVertexData),
                offset * sizeof(TransformVertexData));
      offset++;
    }
  }
  std::vector<TransformVertexData> output;
  output.resize(offset);

  tb->read(std::span{output});
  vb->write(std::span{output});

  command_buffer->begin(current_frame);
  {
    begin_renderpass(*shadow_framebuffer);
    shadow_pass();
    end_renderpass();
  }
  {
    begin_renderpass(*geometry_framebuffer);
    environment_pass();
    light_culling_pass();
    geometry_pass();
    debug_pass();

    bloom_pass();

    // grid_pass();
    end_renderpass();
  }
  {
    begin_renderpass(*fullscreen_framebuffer);
    fullscreen_pass();
    end_renderpass();
  }
  command_buffer->end_and_submit();

  draw_commands.clear();
  shadow_draw_commands.clear();
  mesh_transform_map.clear();
  geometry_renderer.clear();
}

auto SceneRenderer::end_frame() -> void {}

void SceneRenderer::push_constants(const GraphicsPipeline &pipeline,
                                   const Material &material) {

  const auto &constant_buffer = material.get_constant_buffer();
  if (!constant_buffer.valid()) {
    return;
  }

  vkCmdPushConstants(command_buffer->get_command_buffer(),
                     pipeline.get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
                     constant_buffer.size(), constant_buffer.raw());
}
void SceneRenderer::push_constants(const ComputePipeline &pipeline,
                                   const Material &material) {

  const auto &constant_buffer = material.get_constant_buffer();
  if (!constant_buffer.valid()) {
    return;
  }

  vkCmdPushConstants(command_buffer->get_command_buffer(),
                     pipeline.get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
                     constant_buffer.size(), constant_buffer.raw());
}

void SceneRenderer::update_material_for_rendering(
    const FrameIndex frame_index, Material &material_for_update,
    BufferSet<Buffer::Type::Uniform> *ubo_set,
    BufferSet<Buffer::Type::Storage> *sbo_set) {

  if (ubo_set == nullptr) {
    material_for_update.update_for_rendering(frame_index);
    return;
  }

  auto write_descriptors = create_or_get_write_descriptor_for(
      Config::frame_count, ubo_set, material_for_update);

  if (sbo_set != nullptr) {

    if (const auto &storage_buffer_write_sets =
            create_or_get_write_descriptor_for(Config::frame_count, sbo_set,
                                               material_for_update);
        !storage_buffer_write_sets.empty()) {
      for (u32 frame = 0; frame < Config::frame_count; ++frame) {
        auto &ubo_frame_descriptors = write_descriptors[frame];
        const auto &sbo_frame_descriptors = storage_buffer_write_sets[frame];

        ubo_frame_descriptors.insert(ubo_frame_descriptors.end(),
                                     sbo_frame_descriptors.begin(),
                                     sbo_frame_descriptors.end());
      }
    }
  }

  material_for_update.update_for_rendering(frame_index, write_descriptors);
}

auto SceneRenderer::get_output_image() const -> const Image & {
  return *fullscreen_framebuffer->get_image(0);
}

auto SceneRenderer::get_depth_image() const -> const Image & {
  return *shadow_framebuffer->get_depth_image();
}

auto SceneRenderer::create(const Swapchain &swapchain) -> void {
  Shader::initialise_compiler(
      *device,
      Compilation::ShaderCompilerConfiguration{
          .optimisation_level = 0,
          .debug_information_level = Compilation::DebugInformationLevel::None,
          .warnings_as_errors = false,
          .include_directories = {FS::shader_directory()},
          .macro_definitions = {},
      });

  DataBuffer white_data(sizeof(u32));
  u32 white = 0xFFFFFFFF;
  white_data.write(&white, sizeof(u32));
  white_texture = Texture::construct_from_buffer(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .extent =
              {
                  1,
                  1,
              },
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .address_mode = SamplerAddressMode::ClampToEdge,
          .border_color = SamplerBorderColor::FloatOpaqueWhite,
      },
      std::move(white_data));

  DataBuffer black_data(sizeof(u32));
  u32 black = 0;
  black_data.write(&black, sizeof(u32));
  black_texture = Texture::construct_from_buffer(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .extent =
              {
                  1,
                  1,
              },
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .address_mode = SamplerAddressMode::ClampToEdge,
          .border_color = SamplerBorderColor::FloatOpaqueBlack,
      },
      std::move(black_data));

  brdf_lookup_texture = Texture::construct_shader(
      *device,
      TextureProperties{
          .format = ImageFormat::UNORM_RGBA8,
          .path = FS::texture("BRDF_LUT.tga"),
          .address_mode = SamplerAddressMode::ClampToEdge,
          .mip_generation = MipGeneration(MipGenerationStrategy::FromSize),
      });

  extent = swapchain.get_extent();
  inverse_extent = {
      .width = 1.0F / static_cast<float>(extent.width),
      .height = 1.0F / static_cast<float>(extent.height),
  };

  // This preloads some basic meshes. :)
  Mesh::reference_import_from(*device, FS::model("cube.gltf"));
  Mesh::reference_import_from(*device, FS::model("sphere.fbx"));

  const auto default_vertex_layout = VertexLayout{
      {
          LayoutElement{ElementType::Float3, "pos"},
          LayoutElement{ElementType::Float2, "uvs"},
          LayoutElement{ElementType::Float4, "colour"},
          LayoutElement{ElementType::Float3, "normals"},
          LayoutElement{ElementType::Float3, "tangents"},
          LayoutElement{ElementType::Float3, "bitangents"},
      },
      VertexBinding{0},
  };

  const auto default_instance_layout = VertexLayout{
      {
          LayoutElement{ElementType::Float4, "row_zero"},
          LayoutElement{ElementType::Float4, "row_one"},
          LayoutElement{ElementType::Float4, "row_two"},
          LayoutElement{ElementType::Float4, "instance_colour"},
      },
      VertexBinding{1, sizeof(TransformVertexData)},
  };

  shader_cache.try_emplace(
      "PreethamSky",
      Shader::compile_compute_scoped(*device, FS::shader("PreethamSky.comp")));

  buffer_for_transform_data.transforms.resize(100 *
                                              Config::transform_buffer_size);

  command_buffer =
      CommandBuffer::construct(*device, CommandBufferProperties{
                                            .queue_type = Queue::Type::Graphics,
                                            .count = Config::frame_count,
                                            .is_primary = true,
                                            .owned_by_swapchain = false,
                                            .record_stats = true,
                                        });

  compute_command_buffer =
      CommandBuffer::construct(*device, CommandBufferProperties{
                                            .queue_type = Queue::Type::Compute,
                                            .count = Config::frame_count,
                                            .is_primary = true,
                                            .owned_by_swapchain = false,
                                            .record_stats = true,
                                        });
  auto vector = Random::vec3(-2 * glm::pi<float>(), 2 * glm::pi<float>());
  auto texture_cube = create_preetham_sky(3.1, vector.y, vector.z);
  scene_environment.radiance_texture = texture_cube;
  scene_environment.irradiance_texture = texture_cube;

  FramebufferProperties props{
      .width = swapchain.get_extent().width,
      .height = swapchain.get_extent().height,
      .clear_colour =
          {
              0.1F,
              0.1F,
              0.1F,
              1.0F,
          },
      .depth_clear_value = 0.0F,
      .clear_colour_on_load = true,
      .clear_depth_on_load = true,
      .blend = true,
      .attachments =
          FramebufferAttachmentSpecification{
              FramebufferTextureSpecification{
                  .format = ImageFormat::SRGB_RGBA32,
              },
              FramebufferTextureSpecification{
                  .format = ImageFormat::DEPTH24STENCIL8,
                  .blend = false,
              },
          },
      .debug_name = "DefaultFramebuffer",
  };
  geometry_framebuffer = Framebuffer::construct(*device, props);

  FramebufferProperties shadow_props{
      .width = Config::shadow_map_size,
      .height = Config::shadow_map_size,
      .resizeable = false,
      .depth_clear_value = 0.0F,
      .blend = false,
      .attachments =
          FramebufferAttachmentSpecification{
              FramebufferTextureSpecification{
                  .format = ImageFormat::DEPTH32F,
              },
          },
      .flip_viewport = false,
      .debug_name = "ShadowFramebuffer",
  };
  shadow_framebuffer = Framebuffer::construct(*device, shadow_props);

  shader_cache.try_emplace("Shadow", Shader::compile_graphics_scoped(
                                         *device, FS::shader("Shadow.vert"),
                                         FS::shader("Shadow.frag")));
  shadow_material = Material::construct(*device, *shader_cache.at("Shadow"));
  shader_cache.try_emplace(
      "BasicGeometry",
      Shader::compile_graphics_scoped(*device, FS::shader("Basic.vert"),
                                      FS::shader("Basic.frag")));

  GraphicsPipelineConfiguration config{
      .name = "DefaultGraphicsPipeline",
      .shader = shader_cache.at("BasicGeometry").get(),
      .framebuffer = geometry_framebuffer.get(),
      .layout = default_vertex_layout,
      .instance_layout = default_instance_layout,
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  geometry_pipeline = GraphicsPipeline::construct(*device, config);
  config.polygon_mode = PolygonMode::Line;
  config.name = "WireframeGeometry";
  wireframed_geometry_pipeline = GraphicsPipeline::construct(*device, config);

  shader_cache.try_emplace(
      "Grid", Shader::compile_graphics_scoped(*device, FS::shader("Grid.vert"),
                                              FS::shader("Grid.frag")));
  grid_material = Material::construct(*device, *shader_cache.at("Grid"));
  GraphicsPipelineConfiguration grid_config{
      .name = "GridPipeline",
      .shader = shader_cache.at("Grid").get(),
      .framebuffer = geometry_framebuffer.get(),
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Front,
      .face_mode = FaceMode::CounterClockwise,
  };
  grid_pipeline = GraphicsPipeline::construct(*device, grid_config);

  GraphicsPipelineConfiguration shadow_config{
      .name = "ShadowGraphicsPipeline",
      .shader = shader_cache.at("Shadow").get(),
      .framebuffer = shadow_framebuffer.get(),
      .layout = default_vertex_layout,
      .instance_layout = default_instance_layout,
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Front,
      .face_mode = FaceMode::CounterClockwise,
  };
  shadow_pipeline = GraphicsPipeline::construct(*device, shadow_config);

  shader_cache.try_emplace(
      "Fullscreen",
      Shader::compile_graphics_scoped(*device, FS::shader("Fullscreen.vert"),
                                      FS::shader("Fullscreen.frag")));
  fullscreen_material =
      Material::construct(*device, *shader_cache.at("Fullscreen"));
  FramebufferProperties fullscreen_props{
      .width = swapchain.get_extent().width,
      .height = swapchain.get_extent().height,
      .clear_colour = {0.1F, 0.1F, 0.1F, 1.0F},
      .depth_clear_value = 0.0F,
      .blend = true,
      .attachments =
          FramebufferAttachmentSpecification{
              FramebufferTextureSpecification{
                  .format = ImageFormat::SRGB_RGBA32,
              },
          },
      .debug_name = "FullscreenFramebuffer",
  };
  fullscreen_framebuffer = Framebuffer::construct(*device, fullscreen_props);
  GraphicsPipelineConfiguration fullscreen_config{
      .name = "FullscreenPipeline",
      .shader = shader_cache.at("Fullscreen").get(),
      .framebuffer = fullscreen_framebuffer.get(),
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::None,
      .face_mode = FaceMode::CounterClockwise,
  };
  fullscreen_pipeline = GraphicsPipeline::construct(*device, fullscreen_config);

  shader_cache.try_emplace("Skybox", Shader::compile_graphics_scoped(
                                         *device, FS::shader("Skybox.vert"),
                                         FS::shader("Skybox.frag")));
  GraphicsPipelineConfiguration skybox_config{
      .name = "FullscreenPipeline",
      .shader = shader_cache.at("Skybox").get(),
      .framebuffer = geometry_framebuffer.get(),
      .depth_comparison_operator = DepthCompareOperator::Greater,
      .write_depth = false,
      .test_depth = true,
  };
  skybox_material = Material::construct(*device, *shader_cache.at("Skybox"));
  skybox_pipeline = GraphicsPipeline::construct(*device, skybox_config);

  ubos = make_scope<BufferSet<Buffer::Type::Uniform>>(*device);
  ubos->create(sizeof(RendererUBO), SetBinding(0));
  ubos->create(sizeof(ShadowUBO), SetBinding(1));
  ubos->create(sizeof(GridUBO), SetBinding(3));
  ubos->create(sizeof(PointLights), SetBinding(4));
  ubos->create(sizeof(SpotLights), SetBinding(5));
  ubos->create(sizeof(SpotShadows), SetBinding(6));
  ubos->create(sizeof(ScreenData), SetBinding(9));
  ssbos = make_scope<BufferSet<Buffer::Type::Storage>>(*device);
  ssbos->create(buffer_for_transform_data.size(), SetBinding(2));

  {
    // Point/spot lights + culled lights
    ssbos->create(1, SetBinding(7));
    ssbos->create(1, SetBinding(8));
    shader_cache.try_emplace("LightCulling",
                             Shader::compile_compute_scoped(
                                 *device, FS::shader("LightCulling.comp")));
    light_culling_material =
        Material::construct(*device, *shader_cache.at("LightCulling"));
    light_culling_pipeline = ComputePipeline::construct(
        *device, {"LightCulling", PipelineStage::Compute,
                  *shader_cache.at("LightCulling")});
  }

  transform_buffers.resize(Config::frame_count);
  static constexpr auto total_size =
      100 * Config::transform_buffer_size * sizeof(TransformVertexData);
  for (auto &[vertex_buffer, transform_buffer] : transform_buffers) {
    vertex_buffer =
        Buffer::construct(*device, total_size, Buffer::Type::Vertex);
    transform_buffer = make_scope<DataBuffer>(total_size);
    transform_buffer->fill_zero();
  }

  create_pool_and_layout();

  shader_cache.try_emplace("Bloom", Shader::compile_compute_scoped(
                                        *device, FS::shader("Bloom.comp")));
  bloom_pipeline = ComputePipeline::construct(
      *device, {"Bloom", PipelineStage::Compute, *shader_cache.at("Bloom")});
  bloom_material = Material::construct(*device, *shader_cache.at("Bloom"));

  TextureProperties bloom_texture_props;
  bloom_texture_props.address_mode = SamplerAddressMode::ClampToBorder;
  bloom_texture_props.extent = {1, 1};
  bloom_texture_props.format = ImageFormat::SRGB_RGBA32;
  bloom_texture_props.usage = ImageUsage::Storage | ImageUsage::TransferSrc |
                              ImageUsage::TransferDst | ImageUsage::Sampled;
  bloom_texture_props.layout = ImageLayout::General;
  bloom_texture_props.mip_generation =
      MipGeneration(MipGenerationStrategy::FromSize);
  {
    DataBuffer buffer(4 * sizeof(float));
    buffer.fill_zero();
    bloom_texture_props.identifier = "bloom_compute_0";
    bloom_textures.at(0) = Texture::construct_from_buffer(
        *device, bloom_texture_props, std::move(buffer));
  }
  {
    DataBuffer buffer(4 * sizeof(float));
    buffer.fill_zero();
    bloom_texture_props.identifier = "bloom_compute_1";
    bloom_textures.at(1) = Texture::construct_from_buffer(
        *device, bloom_texture_props, std::move(buffer));
  }
  {
    DataBuffer buffer(4 * sizeof(float));
    buffer.fill_zero();
    bloom_texture_props.identifier = "bloom_compute_2";
    bloom_textures.at(2) = Texture::construct_from_buffer(
        *device, bloom_texture_props, std::move(buffer));
  }

  geometry_renderer.create(*geometry_framebuffer, 100);
}

auto SceneRenderer::create_pool_and_layout() -> void {
  std::array<VkDescriptorPoolSize, 4> pool_sizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           10 * Config::frame_count},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 * Config::frame_count},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 * Config::frame_count},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10 * Config::frame_count},
  };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = static_cast<u32>(pool_sizes.size()) * 4UL;
  pool_info.poolSizeCount = static_cast<u32>(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes.data();

  verify(
      vkCreateDescriptorPool(device->get_device(), &pool_info, nullptr, &pool),
      "vkCreateDescriptorPool", "Failed to create descriptor pool");

  const auto ubo_bindings = ubos->get_bindings();
  const auto sbo_bindings = ssbos->get_bindings();

  auto combined = Container::combine_into_one(ubo_bindings, sbo_bindings);

  const VkDescriptorSetLayoutCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<u32>(combined.size()),
      .pBindings = combined.data(),
  };

  vkCreateDescriptorSetLayout(device->get_device(), &create_info, nullptr,
                              &layout);
}

} // namespace Core
