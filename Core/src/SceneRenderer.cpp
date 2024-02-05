#include "pch/vkgpgpu_pch.hpp"

#include "SceneRenderer.hpp"

#include <glm/glm.hpp>

namespace Core {

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
    for (const auto &binding : storage_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
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

auto SceneRenderer::on_resize(const Extent<u32> &new_extent) -> void {
  extent = new_extent;
  geometry_framebuffer->on_resize(extent);
  shadow_framebuffer->on_resize(extent);
  fullscreen_framebuffer->on_resize(extent);

  shadow_material->on_resize(extent);
  grid_material->on_resize(extent);
  fullscreen_material->on_resize(extent);
}

auto SceneRenderer::destroy() -> void {
  Destructors::destroy(device, pool);
  Destructors::destroy(device, layout);

  white_texture.reset();
  black_texture.reset();
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
  begin_renderpass(framebuffer);

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
    attachments[color_attachment_count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
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

  end_renderpass();
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

    auto &command = draw_commands[key];
    command.mesh_ptr = mesh;
    command.submesh_index = submesh;
    command.instance_count++;
    command.material = mesh->get_material(submesh);
    command.colours.push_back(colour);

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

auto SceneRenderer::begin_frame(const glm::mat4 &projection,
                                const glm::mat4 &view) -> void {
  const auto &ubo = ubos->get(0, current_frame);
  renderer_ubo.projection = projection;
  renderer_ubo.view = view;
  renderer_ubo.view_projection = renderer_ubo.projection * renderer_ubo.view;
  renderer_ubo.light_position = {sun_position, 1.0F};
  renderer_ubo.light_direction = {glm::normalize(-sun_position), 1.0F};
  const auto &position = view[3];
  renderer_ubo.camera_position = position;

  ubo->write(renderer_ubo);

  const auto &ubo_shadow = ubos->get(1, current_frame);
  shadow_ubo.projection =
      glm::ortho(-depth_factor.value, depth_factor.value, -depth_factor.value,
                 depth_factor.value, depth_factor.near, depth_factor.far);
  shadow_ubo.view = glm::lookAt(sun_position, {0, 0, 0}, {0, -1, 0});
  shadow_ubo.view_projection = shadow_ubo.projection * shadow_ubo.view;
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
    auto &&[mesh_ptr, submesh_index, instance_count, colour_data, material] =
        value;

    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    if (material) {
      update_material_for_rendering(current_frame, *material, ubos.get(),
                                    ssbos.get());
      material->bind(*command_buffer, *shadow_pipeline, current_frame);
    }

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
}

auto SceneRenderer::geometry_pass() -> void {
  bind_pipeline(*geometry_pipeline);
  const auto &colours = ssbos->get(4, current_frame);
  for (auto &&[key, value] : draw_commands) {
    auto &&[mesh_ptr, submesh_index, instance_count, colour_data, material] =
        value;
    colours->write(colour_data.data(), colour_data.size() * sizeof(glm::vec4));

    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    if (material) {
      material->set("shadow_map", *shadow_framebuffer->get_depth_image());
      update_material_for_rendering(current_frame, *material, ubos.get(),
                                    ssbos.get());
      material->bind(*command_buffer, *geometry_pipeline, current_frame);
      push_constants(*geometry_pipeline, *material);
    }

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
}

auto SceneRenderer::debug_pass() -> void {
  // AABBs with cube_mesh, from debug_draw_commands
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

  fullscreen_material->set("colour_texture",
                           *geometry_framebuffer->get_image(0));

  update_material_for_rendering(current_frame, *fullscreen_material, ubos.get(),
                                ssbos.get());
  fullscreen_material->bind(*command_buffer, *fullscreen_pipeline,
                            current_frame);
  draw_vertices({
      .vertex_count = 3,
  });
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
    geometry_pass();
    debug_pass();
    grid_pass();
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

void SceneRenderer::update_material_for_rendering(
    FrameIndex frame_index, Material &material_for_update,
    BufferSet<Buffer::Type::Uniform> *ubo_set,
    BufferSet<Buffer::Type::Storage> *sbo_set) {

  if (ubo_set == nullptr) {
    material_for_update.update_for_rendering(frame_index);
    return;
  }

  auto write_descriptors =
      create_or_get_write_descriptor_for<Buffer::Type::Uniform>(
          Config::frame_count, ubo_set, material_for_update);

  if (sbo_set != nullptr) {
    const auto &storage_buffer_write_sets =
        create_or_get_write_descriptor_for<Buffer::Type::Storage>(
            Config::frame_count, sbo_set, material_for_update);

    if (!storage_buffer_write_sets.empty()) {
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
  extent = swapchain.get_extent();

  const VertexLayout default_vertex_layout = VertexLayout{
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

  const VertexLayout default_instance_layout = VertexLayout{
      {
          LayoutElement{ElementType::Float4, "row_zero"},
          LayoutElement{ElementType::Float4, "row_one"},
          LayoutElement{ElementType::Float4, "row_two"},
      },
      VertexBinding{1, sizeof(TransformVertexData)},
  };

  buffer_for_transform_data.transforms.resize(100 *
                                              Config::transform_buffer_size);
  buffer_for_colour_data.colours.resize(100 * Config::transform_buffer_size);

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

  FramebufferProperties props{
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
              FramebufferTextureSpecification{
                  .format = ImageFormat::DEPTH32F,
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
      },
      std::move(white_data));

  disarray_texture = Texture::construct(*device, FS::texture("D.png"));

  shadow_shader = Shader::construct(*device, FS::shader("Shadow.vert.spv"),
                                    FS::shader("Shadow.frag.spv"));
  shadow_material = Material::construct(*device, *shadow_shader);
  geometry_shader = Shader::construct(*device, FS::shader("Basic.vert.spv"),
                                      FS::shader("Basic.frag.spv"));

  GraphicsPipelineConfiguration config{
      .name = "DefaultGraphicsPipeline",
      .shader = geometry_shader.get(),
      .framebuffer = geometry_framebuffer.get(),
      .layout = default_vertex_layout,
      .instance_layout = default_instance_layout,
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  geometry_pipeline = GraphicsPipeline::construct(*device, config);
  config.polygon_mode = PolygonMode::Line;
  wireframed_geometry_pipeline = GraphicsPipeline::construct(*device, config);

  grid_shader = Shader::construct(*device, FS::shader("Grid.vert.spv"),
                                  FS::shader("Grid.frag.spv"));
  grid_material = Material::construct(*device, *grid_shader);
  GraphicsPipelineConfiguration grid_config{
      .name = "GridPipeline",
      .shader = grid_shader.get(),
      .framebuffer = geometry_framebuffer.get(),
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Front,
      .face_mode = FaceMode::CounterClockwise,
  };
  grid_pipeline = GraphicsPipeline::construct(*device, grid_config);
  cube_mesh = Mesh::import_from(*device, FS::model("cube.fbx"));

  GraphicsPipelineConfiguration shadow_config{
      .name = "ShadowGraphicsPipeline",
      .shader = shadow_shader.get(),
      .framebuffer = shadow_framebuffer.get(),
      .layout = default_vertex_layout,
      .instance_layout = default_instance_layout,
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  shadow_pipeline = GraphicsPipeline::construct(*device, shadow_config);

  fullscreen_shader =
      Shader::construct(*device, FS::shader("Triangle.vert.spv"),
                        FS::shader("Triangle.frag.spv"));
  fullscreen_material = Material::construct(*device, *fullscreen_shader);
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
      .shader = fullscreen_shader.get(),
      .framebuffer = fullscreen_framebuffer.get(),
      .depth_comparison_operator = DepthCompareOperator::GreaterOrEqual,
      .cull_mode = CullMode::None,
      .face_mode = FaceMode::CounterClockwise,
  };
  fullscreen_pipeline = GraphicsPipeline::construct(*device, fullscreen_config);

  ubos = make_scope<BufferSet<Buffer::Type::Uniform>>(*device);
  ubos->create(sizeof(RendererUBO), SetBinding(0));
  ubos->create(sizeof(ShadowUBO), SetBinding(1));
  ubos->create(sizeof(GridUBO), SetBinding(3));
  ssbos = make_scope<BufferSet<Buffer::Type::Storage>>(*device);
  ssbos->create(buffer_for_transform_data.size(), SetBinding(2));
  ssbos->create(buffer_for_colour_data.size(), SetBinding(4));

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
