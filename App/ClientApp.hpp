#pragma once

#include "Allocator.hpp"
#include "App.hpp"
#include "Buffer.hpp"
#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorResource.hpp"
#include "Destructors.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Framebuffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Material.hpp"
#include "Math.hpp"
#include "Pipeline.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Timer.hpp"
#include "Types.hpp"

#include <compare>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <random>
#include <unordered_map>

#include "bus/MessagingClient.hpp"
#include "widgets/Widget.hpp"

using namespace Core;

struct Mesh {
  struct Vertex {
    glm::vec3 pos;
    glm::vec2 uvs;
    glm::vec4 colour{1.0F};
    glm::vec3 normals{0.F};
    glm::vec3 tangents{0.F};
    glm::vec3 bitangents{0.F};
  };

  struct Submesh {
    u32 base_vertex;
    u32 base_index;
    u32 material_index;
    u32 index_count;
    u32 vertex_count;

    // glm::mat4 transform{ 1.0f }; // World transform
    // glm::mat4 local_transform{ 1.0f };
    // AABB bounding_box;
  };
  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  std::vector<Scope<Material>> materials;

  std::vector<Submesh> submeshes;
  std::vector<u32> submesh_indices;
  auto get_submeshes() const -> const auto & { return submesh_indices; }
  auto get_submesh(u32 index) const -> const auto & {
    return submeshes.at(index);
  }
  auto get_materials_span() const {
    std::vector<Material *> raw_pointers;
    raw_pointers.reserve(materials.size());
    for (auto &uniquePtr : materials) {
      raw_pointers.push_back(uniquePtr.get());
    }

    return std::span<Material *>(raw_pointers.data(), raw_pointers.size());
  }
  auto get_material(u32 index) const -> Material * {
    if (materials.size() < index && index > 0) {
      return materials.at(index).get();
    }
    return nullptr;
  }

  static auto cube(const Device &) -> Scope<Mesh>;
  static auto quad() -> Scope<Mesh>;
  static auto line() -> Scope<Mesh>;
};

struct CommandKey {
  const Mesh *mesh_ptr{nullptr};
  u32 submesh_index{0};

  auto operator<=>(const CommandKey &) const = default;
};

namespace std {
template <> struct hash<CommandKey> {
  auto operator()(const CommandKey &key) const {
    static constexpr std::hash<u32> int_hasher;
    static constexpr std::hash<const Mesh *> pointer_hasher;

    return int_hasher(key.submesh_index) ^ pointer_hasher(key.mesh_ptr);
  }
};
} // namespace std

class SceneRenderer {
  struct DrawCommand {
    const Mesh *mesh_ptr{};
    u32 submesh_index{0};
    std::vector<glm::mat4> transforms_and_instances{};
    Material *material{};
  };

  struct RendererUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
  };
  struct TransformData {
    std::array<glm::mat4, 1000000> transforms{};
  };
  static constexpr auto size = sizeof(TransformData);
  TransformData transform_data;
  RendererUBO renderer_ubo;

  Scope<BufferSet<Buffer::Type::Uniform>> ubos;
  Scope<BufferSet<Buffer::Type::Storage>> ssbos;

  VkDescriptorPool pool;
  VkDescriptorSet active = nullptr;
  VkDescriptorSetLayout layout = nullptr;

public:
  void destroy(const Device &device) {
    Destructors::destroy(device, pool);
    Destructors::destroy(device, layout);
  }

  auto begin_renderpass(const CommandBuffer &buffer,
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

  struct DrawParameters {
    u32 index_count;
    u32 instance_count{1};
    u32 first_index{0};
    u32 vertex_offset{0};
    u32 first_instance{0};
  };
  auto draw(const CommandBuffer &buffer, const DrawParameters &params) {
    vkCmdDrawIndexed(buffer.get_command_buffer(), params.index_count,
                     params.instance_count, params.first_index,
                     params.vertex_offset, params.first_instance);
  }

  auto bind_pipeline(const CommandBuffer &buffer,
                     const GraphicsPipeline &pipeline) {
    pipeline.bind(buffer);
    bound_pipeline = {
        .bound_pipeline = pipeline.get_pipeline(),
        .hash = pipeline.hash(),
    };
  }

  auto bind_index_buffer(const CommandBuffer &buffer,
                         const Buffer &index_buffer) {
    vkCmdBindIndexBuffer(buffer.get_command_buffer(), index_buffer.get_buffer(),
                         0, VK_INDEX_TYPE_UINT32);
  }

  auto bind_vertex_buffer(const CommandBuffer &buffer,
                          const Buffer &vertex_buffer) {
    const std::array<VkDeviceSize, 1> offset = {0};
    const std::array buffers = {vertex_buffer.get_buffer()};
    vkCmdBindVertexBuffers(buffer.get_command_buffer(), 0, 1, buffers.data(),
                           offset.data());
  }

  auto submit_static_mesh(const Mesh *mesh, const glm::mat4 &transform = {}) {
    for (const auto &submesh : mesh->get_submeshes()) {
      CommandKey key{mesh, submesh};

      auto &command = draw_commands[key];
      command.mesh_ptr = mesh;
      command.submesh_index = submesh;
      command.transforms_and_instances.push_back(transform);
      command.material = mesh->get_material(submesh);
    }
  }

  auto end_renderpass(const CommandBuffer &buffer, u32 frame = 0) -> void {
    for (const auto &[key, value] : draw_commands) {
      bind_pipeline(buffer, *geometry_pipeline);

      if (value.material) {
        auto &material = *value.material;
        material.bind(buffer, *geometry_pipeline, frame, active);
      }
      auto &ssbo = ssbos->get(1, frame, 2);
      ssbo->write(value.transforms_and_instances.data(),
                  value.transforms_and_instances.size() * sizeof(glm::mat4));

      vkCmdBindDescriptorSets(
          buffer.get_command_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          geometry_pipeline->get_pipeline_layout(), 2, 1, &active, 0, nullptr);

      bind_index_buffer(buffer, *value.mesh_ptr->index_buffer);
      bind_vertex_buffer(buffer, *value.mesh_ptr->vertex_buffer);
      const auto &submesh_specification =
          value.mesh_ptr->get_submesh(value.submesh_index);

      draw(buffer, {
                       .index_count = submesh_specification.index_count,
                       .instance_count = static_cast<u32>(
                           value.transforms_and_instances.size()),
                       .first_index = submesh_specification.base_index,
                   });
    }

    vkCmdEndRenderPass(buffer.get_command_buffer());

    bound_pipeline.reset();
    draw_commands.clear();
  }

  auto create(const Device &device, const Swapchain &swapchain) {
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

    geometry_shader = Shader::construct(device, FS::shader("Basic.vert.spv"),
                                        FS::shader("Basic.frag.spv"));

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

    std::array bindings{ubo_binding, ssbo_binding};
    VkDescriptorSetLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };

    vkCreateDescriptorSetLayout(device.get_device(), &create_info, nullptr,
                                &layout);

    ubos = make_scope<BufferSet<Buffer::Type::Uniform>>(device);
    ubos->create(sizeof(RendererUBO), SetBinding(2, 0));
    ssbos = make_scope<BufferSet<Buffer::Type::Storage>>(device);
    ssbos->create(sizeof(TransformData), SetBinding(2, 1));
  }

  auto begin_frame(const Device &device, u32 frame,
                   const glm::vec3 &camera_position) -> void {
    vkResetDescriptorPool(device.get_device(), pool, 0);

    VkDescriptorSetAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation_info.descriptorPool = this->pool;
    allocation_info.descriptorSetCount = 1;
    allocation_info.pSetLayouts = &layout;
    vkAllocateDescriptorSets(device.get_device(), &allocation_info, &active);

    auto &ubo = ubos->get(0, frame, 2);
    renderer_ubo.projection =
        glm::perspective(glm::radians(70.0F), 1280.0F / 720.0F, 0.1F, 1000.0F);
    renderer_ubo.view = glm::lookAt(camera_position, {0, 0, 0}, {0, -1, 0});
    renderer_ubo.view_projection = renderer_ubo.projection * renderer_ubo.view;

    ubo->write(renderer_ubo);

    VkWriteDescriptorSet ubo_write = {};
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.descriptorCount = 1;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.dstSet = active;
    ubo_write.dstBinding = 0;
    ubo_write.pBufferInfo = &ubo->get_descriptor_info();

    auto &ssbo = ssbos->get(1, frame, 2);

    VkWriteDescriptorSet ssbo_write = {};
    ssbo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssbo_write.descriptorCount = 1;
    ssbo_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssbo_write.dstSet = active;
    ssbo_write.dstBinding = 1;
    ssbo_write.pBufferInfo = &ssbo->get_descriptor_info();

    std::array writes{ubo_write, ssbo_write};
    vkUpdateDescriptorSets(device.get_device(), static_cast<u32>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  auto end_frame() -> void {}

private:
  Scope<GraphicsPipeline> geometry_pipeline;
  Scope<Shader> geometry_shader;
  Scope<Framebuffer> geometry_framebuffer;

  struct PipelineAndHash {
    VkPipeline bound_pipeline{nullptr};
    u64 hash{0};

    auto reset() -> void {
      bound_pipeline = nullptr;
      hash = 0;
    }
  };
  PipelineAndHash bound_pipeline{};

  std::unordered_map<CommandKey, DrawCommand> draw_commands;

  auto is_already_bound(const GraphicsPipeline &pipeline) -> bool {
    return pipeline.hash() == bound_pipeline.hash;
  }
};

class ClientApp : public App {
public:
  explicit ClientApp(const ApplicationProperties &props);
  ~ClientApp() override = default;

  void on_update(floating ts) override;
  void on_resize(const Extent<u32> &) override;
  void on_interface(Core::InterfaceSystem &) override;
  void on_create() override;
  void on_destroy() override;

private:
  Scope<CommandDispatcher> dispatcher;
  Scope<DynamicLibraryLoader> loader;

  glm::vec3 camera_position{-3, -5, 3};

  std::vector<Scope<Widget>> widgets{};

  using UniformSet = BufferSet<Buffer::Type::Uniform>;
  UniformSet uniform_buffer_set;
  using StorageSet = BufferSet<Buffer::Type::Storage>;
  StorageSet storage_buffer_set;

  Timer timer;

  Scope<CommandBuffer> command_buffer;
  Scope<CommandBuffer> graphics_command_buffer;
  Scope<Framebuffer> framebuffer;

  Scope<Material> material;
  Scope<Pipeline> pipeline;
  Scope<Shader> shader;

  Scope<Material> second_material;
  Scope<Pipeline> second_pipeline;
  Scope<Shader> second_shader;

  Scope<Buffer> vertex_buffer;
  Scope<Buffer> index_buffer;
  Scope<Material> graphics_material;
  Scope<GraphicsPipeline> graphics_pipeline;
  Scope<Shader> graphics_shader;

  Scope<Texture> texture;
  Scope<Texture> output_texture;
  Scope<Texture> output_texture_second;

  Scope<Mesh> mesh;
  Scope<Mesh> cube_mesh;
  SceneRenderer scene_renderer{};

  struct PCForMaterial {
    i32 kernel_size{};
    i32 half_size{};
    i32 center_value{};
  };
  PCForMaterial pc{};

  std::array<Math::Mat4, 10> matrices{};

  auto update_entities(floating ts) -> void;
  auto scene_drawing(floating ts) -> void;
  auto compute(floating ts) -> void;
  auto graphics(floating ts) -> void;

  void perform();
  static auto update_material_for_rendering(
      FrameIndex frame_index, Material &material_for_update,
      BufferSet<Buffer::Type::Uniform> *ubo_set,
      BufferSet<Buffer::Type::Storage> *sbo_set) -> void;
  static auto update_material_for_rendering(FrameIndex frame_index,
                                            Material &material_for_update)
      -> void;
};
