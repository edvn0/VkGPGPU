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

auto SceneRenderer::destroy(const Device &device) -> void {
  Destructors::destroy(device, pool);
  Destructors::destroy(device, layout);

  white_texture.reset();
  black_texture.reset();
}

auto SceneRenderer::begin_renderpass(const CommandBuffer &buffer,
                                     const Framebuffer &framebuffer) -> void {
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
  vkCmdBeginRenderPass(buffer.get_command_buffer(), &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Scissors and viewport
  VkViewport viewport = {};
  viewport.x = 0.0F;
  viewport.y = static_cast<float>(framebuffer.get_height());
  viewport.width = static_cast<float>(framebuffer.get_width());
  viewport.height = -static_cast<float>(framebuffer.get_height());
  viewport.minDepth = 1.0F;
  viewport.maxDepth = 0.0F;
  vkCmdSetViewport(buffer.get_command_buffer(), 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.extent.width = framebuffer.get_width();
  scissor.extent.height = framebuffer.get_height();
  vkCmdSetScissor(buffer.get_command_buffer(), 0, 1, &scissor);
}

auto SceneRenderer::explicit_clear(const CommandBuffer &buffer,
                                   const Framebuffer &framebuffer) -> void {
  begin_renderpass(buffer, framebuffer);

  std::vector<VkClearValue> fb_clear_values = framebuffer.get_clear_values();

  const auto color_attachment_count = framebuffer.get_colour_attachment_count();
  const auto total_attachment_count =
      color_attachment_count + (framebuffer.has_depth_attachment() ? 1 : 0);

  VkExtent2D extent_2_d = {
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
      buffer.get_command_buffer(), static_cast<u32>(total_attachment_count),
      attachments.data(), static_cast<u32>(total_attachment_count),
      clear_rects.data());

  end_renderpass(buffer);
}

auto SceneRenderer::draw(const CommandBuffer &buffer,
                         const DrawParameters &params) -> void {
  vkCmdDrawIndexed(buffer.get_command_buffer(), params.index_count,
                   params.instance_count, params.first_index,
                   params.vertex_offset, params.first_instance);
}

auto SceneRenderer::bind_pipeline(const CommandBuffer &buffer,
                                  const GraphicsPipeline &pipeline) -> void {
  pipeline.bind(buffer);
  bound_pipeline = {
      .bound_pipeline = pipeline.get_pipeline(),
      .hash = pipeline.hash(),
  };
}

auto SceneRenderer::bind_index_buffer(const CommandBuffer &buffer,
                                      const Buffer &index_buffer) -> void {
  vkCmdBindIndexBuffer(buffer.get_command_buffer(), index_buffer.get_buffer(),
                       0, VK_INDEX_TYPE_UINT32);
}

auto SceneRenderer::bind_vertex_buffer(const CommandBuffer &buffer,
                                       const Buffer &vertex_buffer) -> void {
  constexpr std::array<VkDeviceSize, 1> offset = {0};
  const std::array buffers = {vertex_buffer.get_buffer()};
  vkCmdBindVertexBuffers(buffer.get_command_buffer(), 0, 1, buffers.data(),
                         offset.data());
}

auto SceneRenderer::submit_static_mesh(const Mesh *mesh,
                                       const glm::mat4 &transform) -> void {
  for (const auto &submesh : mesh->get_submeshes()) {
    CommandKey key{mesh, submesh};

    auto &command = draw_commands[key];
    command.mesh_ptr = mesh;
    command.submesh_index = submesh;
    command.transforms_and_instances.push_back(transform);
    command.material = mesh->get_material(submesh);

    if (mesh->casts_shadows()) {
      auto &shadow_command = shadow_draw_commands[key];
      shadow_command.mesh_ptr = mesh;
      shadow_command.submesh_index = submesh;
      shadow_command.transforms_and_instances.push_back(transform);
      shadow_command.material = shadow_material.get();
    }
  }
}

auto SceneRenderer::end_renderpass(const CommandBuffer &buffer) -> void {
  vkCmdEndRenderPass(buffer.get_command_buffer());
}

auto SceneRenderer::begin_frame(const Device &device, u32 frame,
                                const glm::vec3 &camera_position) -> void {
  vkResetDescriptorPool(device.get_device(), pool, 0);

  // For now
  const auto position = glm::translate(glm::mat4{1.0F}, sun_position);
  const auto scale = glm::scale(glm::mat4{1.0F}, glm::vec3{1000.0F});
  const auto transformation = position * scale;
  submit_static_mesh(sphere_mesh.get(), transformation);

  VkDescriptorSetAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorPool = pool;
  allocation_info.descriptorSetCount = 1;
  allocation_info.pSetLayouts = &layout;
  vkAllocateDescriptorSets(device.get_device(), &allocation_info, &active);

  VkWriteDescriptorSet ubo_write = {};
  {
    const auto &ubo = ubos->get(0, frame);
    renderer_ubo.projection = glm::perspective(
        glm::radians(45.0F), extent.aspect_ratio(), 0.1F, 1000.0F);
    renderer_ubo.view = glm::lookAt(camera_position, {0, 0, 0}, {0, -1, 0});
    renderer_ubo.view_projection = renderer_ubo.projection * renderer_ubo.view;
    renderer_ubo.light_position = {sun_position, 1.0F};
    renderer_ubo.light_direction = {glm::normalize(-sun_position), 1.0F};
    renderer_ubo.camera_position = {camera_position, 1.0F};

    ubo->write(renderer_ubo);

    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.descriptorCount = 1;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.dstSet = active;
    ubo_write.dstBinding = 0;
    ubo_write.pBufferInfo = &ubo->get_descriptor_info();
  }

  VkWriteDescriptorSet shadow_ubo_write = {};
  {
    const auto &ubo_shadow = ubos->get(1, frame);
    shadow_ubo.projection =
        glm::ortho(-10.0F, 10.0F, -10.0F, 10.0F, 0.1F, 1000.0F);
    shadow_ubo.view = glm::lookAt(sun_position, {0, 0, 0}, {0, -1, 0});
    shadow_ubo.view_projection = shadow_ubo.projection * shadow_ubo.view;

    ubo_shadow->write(shadow_ubo);

    shadow_ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadow_ubo_write.descriptorCount = 1;
    shadow_ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadow_ubo_write.dstSet = active;
    shadow_ubo_write.dstBinding = 2;
    shadow_ubo_write.pBufferInfo = &ubo_shadow->get_descriptor_info();
  }

  VkWriteDescriptorSet grid_ubo_write = {};
  {
    const auto &ubo_grid = ubos->get(3, frame);
    grid_ubo.grid_colour = glm::vec4{0.2F, 0.2F, 0.2F, 1.0F};
    grid_ubo.plane_colour = glm::vec4{0.4F, 0.4F, 0.4F, 1.0F};
    grid_ubo.grid_size = glm::vec4{1.0F, 1.0F, 0.0F, 0.0F};
    grid_ubo.fog_colour = glm::vec4{0.8F, 0.9F, 1.0F, 0.02F};

    ubo_grid->write(grid_ubo);

    grid_ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    grid_ubo_write.descriptorCount = 1;
    grid_ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    grid_ubo_write.dstSet = active;
    grid_ubo_write.dstBinding = 3;
    grid_ubo_write.pBufferInfo = &ubo_grid->get_descriptor_info();
  }

  VkWriteDescriptorSet ssbo_write = {};
  {
    const auto &ssbo = ssbos->get(2, frame);

    ssbo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssbo_write.descriptorCount = 1;
    ssbo_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssbo_write.dstSet = active;
    ssbo_write.dstBinding = 1;
    ssbo_write.pBufferInfo = &ssbo->get_descriptor_info();
  }

  std::array writes{
      ubo_write,
      ssbo_write,
      shadow_ubo_write,
      grid_ubo_write,
  };
  vkUpdateDescriptorSets(device.get_device(), static_cast<u32>(writes.size()),
                         writes.data(), 0, nullptr);
}

auto SceneRenderer::shadow_pass(const CommandBuffer &buffer, u32 frame)
    -> void {
  bind_pipeline(buffer, *shadow_pipeline);
  // update_material_for_rendering(FrameIndex{frame}, *shadow_material,
  // ubos.get(),
  //                              ssbos.get());
  // shadow_material->bind(buffer, *shadow_pipeline, frame);
  for (const auto &[mesh_ptr, submesh_index, transforms_and_instances,
                    material] : shadow_draw_commands | std::views::values) {
    const auto &transforms = ssbos->get(2, frame);
    transforms->write(transforms_and_instances.data(),
                      transforms_and_instances.size() * sizeof(glm::mat4));

    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    if (material) {
      update_material_for_rendering(FrameIndex{frame}, *material, ubos.get(),
                                    ssbos.get());
      material->bind(buffer, *shadow_pipeline, frame);
    }

    bind_vertex_buffer(buffer, *mesh_ptr->get_vertex_buffer());
    bind_index_buffer(buffer, *mesh_ptr->get_index_buffer());

    draw(buffer, {
                     .index_count = submesh.index_count,
                     .instance_count =
                         static_cast<u32>(transforms_and_instances.size()),
                     .first_index = submesh.base_index,
                 });
  }
}

auto SceneRenderer::geometry_pass(const CommandBuffer &buffer, u32 frame)
    -> void {
  bind_pipeline(buffer, *geometry_pipeline);
  // update_material_for_rendering(FrameIndex{frame}, *geometry_material,
  //                               ubos.get(), ssbos.get());
  // geometry_material->bind(buffer, *geometry_pipeline, frame);
  for (auto &&[mesh_ptr, submesh_index, transforms_and_instances, material] :
       draw_commands | std::views::values) {

    const auto &transforms = ssbos->get(2, frame);
    transforms->write(transforms_and_instances.data(),
                      transforms_and_instances.size() * sizeof(glm::mat4));

    const auto &submesh = mesh_ptr->get_submesh(submesh_index);

    if (material) {
      material->set("shadow_map", *shadow_framebuffer->get_depth_image());
      update_material_for_rendering(FrameIndex{frame}, *material, ubos.get(),
                                    ssbos.get());
      material->bind(buffer, *geometry_pipeline, frame);
    }

    bind_vertex_buffer(buffer, *mesh_ptr->get_vertex_buffer());
    bind_index_buffer(buffer, *mesh_ptr->get_index_buffer());

    push_constants(buffer, *geometry_pipeline, *material);

    draw(buffer, {
                     .index_count = submesh.index_count,
                     .instance_count =
                         static_cast<u32>(transforms_and_instances.size()),
                     .first_index = submesh.base_index,
                 });
  }
}

auto SceneRenderer::grid_pass(const CommandBuffer &buffer, u32 frame) -> void {
  bind_pipeline(buffer, *grid_pipeline);
  update_material_for_rendering(FrameIndex{frame}, *grid_material, ubos.get(),
                                ssbos.get());
  grid_material->bind(buffer, *grid_pipeline, frame);
  const auto &grid_submesh = grid_mesh->get_submesh(0);

  bind_vertex_buffer(buffer, *grid_mesh->get_vertex_buffer());
  bind_index_buffer(buffer, *grid_mesh->get_index_buffer());

  push_constants(buffer, *grid_pipeline, *grid_material);

  draw(buffer, {
                   .index_count = grid_submesh.index_count,
                   .instance_count = 1,
                   .first_index = grid_submesh.base_index,
               });
}

auto SceneRenderer::flush(const CommandBuffer &buffer, u32 frame) -> void {
  begin_renderpass(buffer, *shadow_framebuffer);
  shadow_pass(buffer, frame);
  end_renderpass(buffer);

  begin_renderpass(buffer, *geometry_framebuffer);
  grid_pass(buffer, frame);
  geometry_pass(buffer, frame);
  end_renderpass(buffer);

  draw_commands.clear();
  shadow_draw_commands.clear();
}

auto SceneRenderer::end_frame() -> void {}

void SceneRenderer::push_constants(const Core::CommandBuffer &buffer,
                                   const GraphicsPipeline &pipeline,
                                   const Material &material) {

  const auto &constant_buffer = material.get_constant_buffer();
  if (!constant_buffer.valid()) {
    return;
  }

  vkCmdPushConstants(buffer.get_command_buffer(),
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
  return *geometry_framebuffer->get_image(0);
}

auto SceneRenderer::get_depth_image() const -> const Image & {
  return *shadow_framebuffer->get_depth_image();
}

auto SceneRenderer::create(const Device &device, const Swapchain &swapchain)
    -> void {

  FramebufferProperties props{
      .width = swapchain.get_extent().width,
      .height = swapchain.get_extent().height,
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
  geometry_framebuffer = Framebuffer::construct(device, props);

  FramebufferProperties shadow_props{
      .width = 1024,
      .height = 1024,
      .resizeable = false,
      .depth_clear_value = 0.0F,
      .blend = false,
      .attachments =
          FramebufferAttachmentSpecification{
              FramebufferTextureSpecification{
                  .format = ImageFormat::DEPTH32F,
              },
          },
      .debug_name = "ShadowFramebuffer",
  };
  shadow_framebuffer = Framebuffer::construct(device, shadow_props);

  DataBuffer white_data(sizeof(u32));
  u32 white = 0xFFFFFFFF;
  white_data.write(&white, sizeof(u32));
  white_texture = Texture::construct_from_buffer(
      device,
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

  disarray_texture = Texture::construct(device, FS::texture("D.png"));
  sphere_mesh = Mesh::import_from(device, FS::model("sphere.fbx"));

  shadow_shader = Shader::construct(device, FS::shader("Shadow.vert.spv"),
                                    FS::shader("Shadow.frag.spv"));
  shadow_material = Material::construct(device, *shadow_shader);
  geometry_shader = Shader::construct(device, FS::shader("Basic.vert.spv"),
                                      FS::shader("Basic.frag.spv"));

  GraphicsPipelineConfiguration config{
      .name = "DefaultGraphicsPipeline",
      .shader = geometry_shader.get(),
      .framebuffer = geometry_framebuffer.get(),
      .layout =
          VertexLayout{
              LayoutElement{ElementType::Float3, "pos"},
              LayoutElement{ElementType::Float2, "uvs"},
              LayoutElement{ElementType::Float4, "colour"},
              LayoutElement{ElementType::Float3, "normals"},
              LayoutElement{ElementType::Float3, "tangents"},
              LayoutElement{ElementType::Float3, "bitangents"},
          },
      .depth_comparison_operator = DepthCompareOperator::Greater,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  geometry_pipeline = GraphicsPipeline::construct(device, config);

  grid_shader = Shader::construct(device, FS::shader("Grid.vert.spv"),
                                  FS::shader("Grid.frag.spv"));
  grid_material = Material::construct(device, *grid_shader);
  GraphicsPipelineConfiguration grid_config{
      .name = "GridPipeline",
      .shader = grid_shader.get(),
      .framebuffer = geometry_framebuffer.get(),
      .layout =
          VertexLayout{
              LayoutElement{ElementType::Float3, "pos"},
              LayoutElement{ElementType::Float2, "uvs"},
              LayoutElement{ElementType::Float4, "colour"},
              LayoutElement{ElementType::Float3, "normals"},
              LayoutElement{ElementType::Float3, "tangents"},
              LayoutElement{ElementType::Float3, "bitangents"},
          },
      .depth_comparison_operator = DepthCompareOperator::Greater,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  grid_pipeline = GraphicsPipeline::construct(device, grid_config);
  grid_mesh = Mesh::import_from(device, FS::model("cube.fbx"));

  GraphicsPipelineConfiguration shadow_config{
      .name = "ShadowGraphicsPipeline",
      .shader = shadow_shader.get(),
      .framebuffer = shadow_framebuffer.get(),
      .layout =
          VertexLayout{
              LayoutElement{ElementType::Float3, "pos"},
              LayoutElement{ElementType::Float2, "uvs"},
              LayoutElement{ElementType::Float4, "colour"},
              LayoutElement{ElementType::Float3, "normals"},
              LayoutElement{ElementType::Float3, "tangents"},
              LayoutElement{ElementType::Float3, "bitangents"},
          },
      .depth_comparison_operator = DepthCompareOperator::Greater,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  shadow_pipeline = GraphicsPipeline::construct(device, shadow_config);

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
      vkCreateDescriptorPool(device.get_device(), &pool_info, nullptr, &pool),
      "vkCreateDescriptorPool", "Failed to create descriptor pool");

  VkDescriptorSetLayoutBinding ubo_binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
  };

  VkDescriptorSetLayoutBinding ssbo_binding{
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
  };

  VkDescriptorSetLayoutBinding shadow_binding{
      .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
  };

  VkDescriptorSetLayoutBinding grid_binding{
      .binding = 3,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
  };

  std::array bindings{
      ubo_binding,
      ssbo_binding,
      shadow_binding,
      grid_binding,
  };
  VkDescriptorSetLayoutCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<u32>(bindings.size()),
      .pBindings = bindings.data(),
  };

  vkCreateDescriptorSetLayout(device.get_device(), &create_info, nullptr,
                              &layout);

  ubos = make_scope<BufferSet<Buffer::Type::Uniform>>(device);
  ubos->create(sizeof(RendererUBO), SetBinding(0));
  ubos->create(sizeof(ShadowUBO), SetBinding(1));
  ubos->create(sizeof(GridUBO), SetBinding(3));
  ssbos = make_scope<BufferSet<Buffer::Type::Storage>>(device);
  ssbos->create(sizeof(TransformData), SetBinding(2));
}

} // namespace Core
