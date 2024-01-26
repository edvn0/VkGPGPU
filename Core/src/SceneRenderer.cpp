#include "pch/vkgpgpu_pch.hpp"

#include "SceneRenderer.hpp"

namespace Core {

template <Core::Buffer::Type T>
auto create_or_get_write_descriptor_for(u32 frames_in_flight,
                                        Core::BufferSet<T> *buffer_set,
                                        Material &material)
    -> const std::vector<std::vector<VkWriteDescriptorSet>> & {
  static std::unordered_map<
      BufferSet<T> *,
      std::unordered_map<std::size_t,
                         std::vector<std::vector<VkWriteDescriptorSet>>>>
      buffer_set_write_descriptor_cache;

  constexpr auto get_descriptor_set_vector =
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

  if (!vulkan_shader.has_descriptor_set(0) ||
      !vulkan_shader.has_descriptor_set(1)) {
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
  const std::array<VkDeviceSize, 1> offset = {0};
  const std::array buffers = {vertex_buffer.get_buffer()};
  vkCmdBindVertexBuffers(buffer.get_command_buffer(), 0, 1, buffers.data(),
                         offset.data());
}

auto SceneRenderer::submit_static_mesh(const Mesh *mesh,
                                       const glm::mat4 &transform) -> void {
  for (const auto &submesh : mesh->get_submeshes()) {
    CommandKey key{mesh, submesh};

    if (!mesh->casts_shadows()) {
      auto &command = draw_commands[key];
      command.mesh_ptr = mesh;
      command.submesh_index = submesh;
      command.transforms_and_instances.push_back(transform);
      command.material = mesh->get_material(submesh);
    } else {
      auto &command = shadow_draw_commands[key];
      command.mesh_ptr = mesh;
      command.submesh_index = submesh;
      command.transforms_and_instances.push_back(transform);
      command.material = mesh->get_material(submesh);
    }
  }
}

auto SceneRenderer::end_renderpass(const CommandBuffer &buffer, u32 frame)
    -> void {
  vkCmdEndRenderPass(buffer.get_command_buffer());

  bound_pipeline.reset();
  draw_commands.clear();
}

auto SceneRenderer::create(const Device &device, const Swapchain &swapchain)
    -> void {
  FramebufferProperties props{
      .width = swapchain.get_extent().width,
      .height = swapchain.get_extent().height,
      .blend = false,
      .attachments =
          FramebufferAttachmentSpecification{
              FramebufferTextureSpecification{
                  .format = ImageFormat::SRGB_RGBA32,
              },
              FramebufferTextureSpecification{
                  .format = ImageFormat::DEPTH32F,
              },
          },
      .debug_name = "DefaultFramebuffer",
  };
  geometry_framebuffer = Framebuffer::construct(device, props);

  FramebufferProperties shadow_props{
      .width = swapchain.get_extent().width,
      .height = swapchain.get_extent().height,
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

  geometry_shader = Shader::construct(device, FS::shader("Basic.vert.spv"),
                                      FS::shader("Basic.frag.spv"));
  shadow_shader = Shader::construct(device, FS::shader("Shadow.vert.spv"),
                                    FS::shader("Shadow.frag.spv"));

  // graphics_material = Material::construct(device, *geometry_shader);
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
      .depth_comparison_operator = DepthCompareOperator::LessOrEqual,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  geometry_pipeline = GraphicsPipeline::construct(device, config);

  GraphicsPipelineConfiguration shadow_config{
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
      .depth_comparison_operator = DepthCompareOperator::LessOrEqual,
      .cull_mode = CullMode::Back,
      .face_mode = FaceMode::CounterClockwise,
  };
  shadow_pipeline = GraphicsPipeline::construct(device, shadow_config);

  std::array<VkDescriptorPoolSize, 4> pool_sizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           Config::frame_count},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Config::frame_count},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Config::frame_count},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, Config::frame_count},
  };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = static_cast<u32>(pool_sizes.size()) * 4ul;
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

  std::array bindings{
      ubo_binding,
      ssbo_binding,
      shadow_binding,
  };
  VkDescriptorSetLayoutCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<u32>(bindings.size()),
      .pBindings = bindings.data(),
  };

  vkCreateDescriptorSetLayout(device.get_device(), &create_info, nullptr,
                              &layout);

  ubos = make_scope<BufferSet<Buffer::Type::Uniform>>(device);
  ubos->create(sizeof(RendererUBO), SetBinding(2, 0));
  ubos->create(sizeof(ShadowUBO), SetBinding(2, 2));
  ssbos = make_scope<BufferSet<Buffer::Type::Storage>>(device);
  ssbos->create(sizeof(TransformData), SetBinding(2, 1));
}

auto SceneRenderer::begin_frame(const Device &device, u32 frame,
                                const glm::vec3 &camera_position) -> void {
  vkResetDescriptorPool(device.get_device(), pool, 0);

  VkDescriptorSetAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorPool = this->pool;
  allocation_info.descriptorSetCount = 1;
  allocation_info.pSetLayouts = &layout;
  vkAllocateDescriptorSets(device.get_device(), &allocation_info, &active);

  VkWriteDescriptorSet ubo_write = {};
  {
    auto &ubo = ubos->get(0, frame, 2);
    renderer_ubo.projection =
        glm::perspective(glm::radians(70.0F), 1280.0F / 720.0F, 0.1F, 1000.0F);
    renderer_ubo.view = glm::lookAt(camera_position, {0, 0, 0}, {0, -1, 0});
    renderer_ubo.view_projection = renderer_ubo.projection * renderer_ubo.view;

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
    auto &ubo_shadow = ubos->get(2, frame, 2);
    shadow_ubo.projection = glm::ortho(-1, 1, -1, 1);
    shadow_ubo.view = glm::lookAt(camera_position, {0, 0, 0}, {0, -1, 0});
    shadow_ubo.view_projection = shadow_ubo.projection * shadow_ubo.view;

    ubo_shadow->write(shadow_ubo);

    shadow_ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadow_ubo_write.descriptorCount = 1;
    shadow_ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadow_ubo_write.dstSet = active;
    shadow_ubo_write.dstBinding = 2;
    shadow_ubo_write.pBufferInfo = &ubo_shadow->get_descriptor_info();
  }

  VkWriteDescriptorSet ssbo_write = {};
  {
    auto &ssbo = ssbos->get(1, frame, 2);

    ssbo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssbo_write.descriptorCount = 1;
    ssbo_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssbo_write.dstSet = active;
    ssbo_write.dstBinding = 1;
    ssbo_write.pBufferInfo = &ssbo->get_descriptor_info();
  }

  std::array writes{ubo_write, ssbo_write, shadow_ubo_write};
  vkUpdateDescriptorSets(device.get_device(), static_cast<u32>(writes.size()),
                         writes.data(), 0, nullptr);
}

auto SceneRenderer::flush(const CommandBuffer &buffer, u32 frame) -> void {

  for (const auto &[key, value] : shadow_draw_commands) {
    begin_renderpass(buffer, *shadow_framebuffer);
    bind_pipeline(buffer, *shadow_pipeline);

    auto &material = value.material;

    update_material_for_rendering(FrameIndex{frame()}, *material);
    material->bind(*graphics_command_buffer, *graphics_pipeline, frame());

    bind_index_buffer(buffer, *index_buffer);
    bind_vertex_buffer(buffer, *vertex_buffer);
    draw(buffer, {
                     .index_count = 3,
                     .instance_count = 1,
                 });
    end_renderpass(buffer, frame);
  }
}

auto SceneRenderer::end_frame() -> void {}

void SceneRenderer::update_material_for_rendering(
    FrameIndex frame_index, Material &material_for_update,
    BufferSet<Buffer::Type::Uniform> *ubo_set,
    BufferSet<Buffer::Type::Storage> *sbo_set) {
  if (ubo_set != nullptr) {
    auto write_descriptors =
        create_or_get_write_descriptor_for<Buffer::Type::Uniform>(
            Config::frame_count, ubo_set, material_for_update);
    if (sbo_set != nullptr) {
      const auto &storage_buffer_write_sets =
          create_or_get_write_descriptor_for<Buffer::Type::Storage>(
              Config::frame_count, sbo_set, material_for_update);

      for (u32 frame = 0; frame < Config::frame_count; frame++) {
        const auto &sbo_ws = storage_buffer_write_sets[frame];
        const auto reserved_size =
            write_descriptors[frame].size() + sbo_ws.size();
        write_descriptors[frame].reserve(reserved_size);
        write_descriptors[frame].insert(write_descriptors[frame].end(),
                                        sbo_ws.begin(), sbo_ws.end());
      }
    }
    material_for_update.update_for_rendering(frame_index, write_descriptors);
  } else {
    material_for_update.update_for_rendering(frame_index);
  }
}

void SceneRenderer::update_material_for_rendering(
    FrameIndex frame_index, Material &material_for_update) {
  update_material_for_rendering(frame_index, material_for_update, nullptr,
                                nullptr);
}

} // namespace Core
