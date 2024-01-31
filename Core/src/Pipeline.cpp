#include "pch/vkgpgpu_pch.hpp"

#include "Pipeline.hpp"

#include "Device.hpp"
#include "Filesystem.hpp"
#include "Framebuffer.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <filesystem>
#include <fstream>

namespace Core {
namespace PipelineHelpers {

constexpr auto to_vulkan_format(ElementType type) -> VkFormat {
  switch (type) {
  case ElementType::Float:
    return VK_FORMAT_R32_SFLOAT;
  case ElementType::Float2:
    return VK_FORMAT_R32G32_SFLOAT;
  case ElementType::Float3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case ElementType::Float4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case ElementType::Double:
    return VK_FORMAT_R64_SFLOAT;
  case ElementType::Int2:
    return VK_FORMAT_R16G16_SINT;
  case ElementType::Int3:
    return VK_FORMAT_R16G16B16_SINT;
  case ElementType::Int4:
    return VK_FORMAT_R16G16B16A16_SINT;
  case ElementType::Uint:
    return VK_FORMAT_R32_UINT;
  case ElementType::Uint2:
    return VK_FORMAT_R16G16_UINT;
  case ElementType::Uint3:
    return VK_FORMAT_R16G16B16_UINT;
  case ElementType::Uint4:
    return VK_FORMAT_R16G16B16A16_UINT;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing element type"};
}

constexpr auto vk_polygon_mode(PolygonMode mode) -> VkPolygonMode {
  switch (mode) {
  case PolygonMode::Fill:
    return VK_POLYGON_MODE_FILL;
  case PolygonMode::Line:
    return VK_POLYGON_MODE_LINE;
  case PolygonMode::Point:
    return VK_POLYGON_MODE_POINT;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing polygon mode"};
}

constexpr auto vk_polygon_topology(PolygonMode mode) -> VkPrimitiveTopology {
  switch (mode) {
  case PolygonMode::Fill:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case PolygonMode::Line:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case PolygonMode::Point:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing polygon topology"};
}

constexpr auto to_vulkan_comparison(DepthCompareOperator depth_comp_operator)
    -> VkCompareOp {
  switch (depth_comp_operator) {
  case DepthCompareOperator::None:
  case DepthCompareOperator::Never:
    return VK_COMPARE_OP_NEVER;
  case DepthCompareOperator::NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case DepthCompareOperator::Less:
    return VK_COMPARE_OP_LESS;
  case DepthCompareOperator::LessOrEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case DepthCompareOperator::Greater:
    return VK_COMPARE_OP_GREATER;
  case DepthCompareOperator::GreaterOrEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case DepthCompareOperator::Equal:
    return VK_COMPARE_OP_EQUAL;
  case DepthCompareOperator::Always:
    return VK_COMPARE_OP_ALWAYS;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing depth compare operator"};
}

constexpr auto to_vulkan_cull_mode(CullMode cull) -> VkCullModeFlags {
  switch (cull) {
  case CullMode::Front:
    return VK_CULL_MODE_FRONT_BIT;
  case CullMode::Back:
    return VK_CULL_MODE_BACK_BIT;
  case CullMode::Both:
    return VK_CULL_MODE_FRONT_AND_BACK;
  case CullMode::None:
    return VK_CULL_MODE_NONE;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing cull mode"};
}

constexpr auto to_vulkan_face_mode(FaceMode face_mode) -> VkFrontFace {
  switch (face_mode) {
  case FaceMode::Clockwise:
    return VK_FRONT_FACE_CLOCKWISE;
  case FaceMode::CounterClockwise:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  default:
    assert(false);
  }
  throw NotFoundException{"Missing face mode"};
}

auto try_load_pipeline_cache(const std::string &pipeline_name)
    -> std::vector<u8> {
  if (const auto cache_path = FS::pipeline_cache(pipeline_name + ".cache");
      std::filesystem::exists(cache_path)) {
    info("Pipeline cache exists. Loading...");
    auto cache_file = std::ifstream{cache_path};
    if (!cache_file) {
      info("Failed to open pipeline cache file at {}", cache_path.string());
    } else {
      auto cache_data = std::vector<u8>(std::filesystem::file_size(cache_path));
      cache_file.read(std::bit_cast<char *>(cache_data.data()),
                      cache_data.size());
      return cache_data;
    }
  } else {
    info("Pipeline cache does not exist. Creating...");
  }

  return {};
}

auto save_cache(auto &device, auto pipeline_cache, const std::string &name) {
  if (name.empty()) {
    return;
  }

  if (std::filesystem::exists(name + ".cache")) {
    info("Pipeline cache exists. Overwriting...");
  }

  if (FS::mkdir_safe("pipeline_cache")) {
    info("Created folder '{}'.", "pipeline_cache");
  }

  const auto output_path = FS::pipeline_cache(name + ".cache");
  debug("Saving pipeline cache to {}", output_path.string());
  std::ofstream file{output_path.c_str(), std::ios::binary};
  if (!file) {
    info("Failed to open pipeline cache file at {}", name + ".cache");
    return;
  }

  auto pipeline_cache_size = size_t{0};
  verify(vkGetPipelineCacheData(device.get_device(), pipeline_cache,
                                &pipeline_cache_size, nullptr),
         "vkGetPipelineCacheData", "Failed to get pipeline cache data");

  auto pipeline_cache_data = std::vector<u8>(pipeline_cache_size);
  verify(vkGetPipelineCacheData(device.get_device(), pipeline_cache,
                                &pipeline_cache_size,
                                pipeline_cache_data.data()),
         "vkGetPipelineCacheData", "Failed to get pipeline cache data");

  file.write(reinterpret_cast<const char *>(pipeline_cache_data.data()),
             pipeline_cache_data.size());
}
} // namespace PipelineHelpers

auto Pipeline::construct(const Device &dev,
                         const PipelineConfiguration &configuration)
    -> Scope<Pipeline> {
  return Scope<Pipeline>(new Pipeline(dev, configuration));
}

Pipeline::Pipeline(const Device &dev,
                   const PipelineConfiguration &configuration)
    : device(dev), name(configuration.name) {
  construct_pipeline(configuration);
}

Pipeline::~Pipeline() {
  try {
    PipelineHelpers::save_cache(device, pipeline_cache, name);
  } catch (const std::exception &exc) {
    error("Pipeline save_cache exception: {}", exc.what());
  }

  vkDestroyPipelineLayout(device.get_device(), pipeline_layout, nullptr);
  vkDestroyPipelineCache(device.get_device(), pipeline_cache, nullptr);
  vkDestroyPipeline(device.get_device(), pipeline, nullptr);
}

auto Pipeline::bind(const CommandBuffer &command_buffer) -> void {
  vkCmdBindPipeline(command_buffer.get_command_buffer(),
                    static_cast<VkPipelineBindPoint>(PipelineStage::Compute),
                    pipeline);
}

auto Pipeline::construct_pipeline(const PipelineConfiguration &configuration)
    -> void {

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
  pipeline_layout_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  const auto &shader = configuration.shader;
  const auto &layouts = shader.get_descriptor_set_layouts();
  pipeline_layout_create_info.setLayoutCount = static_cast<u32>(layouts.size());
  pipeline_layout_create_info.pSetLayouts = layouts.data();
  const auto &push_constants = shader.get_push_constant_ranges();
  std::vector<VkPushConstantRange> pc_ranges(push_constants.size());
  if (!push_constants.empty()) {
    for (u32 i = 0; i < push_constants.size(); i++) {
      const auto &range = push_constants[i];
      auto &pc_range = pc_ranges[i];

      pc_range.stageFlags = range.shader_stage;
      pc_range.offset = range.offset;
      pc_range.size = range.size;
    }

    pipeline_layout_create_info.pushConstantRangeCount =
        static_cast<Core::u32>(pc_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = pc_ranges.data();
  }

  verify(vkCreatePipelineLayout(device.get_device(),
                                &pipeline_layout_create_info, nullptr,
                                &pipeline_layout),
         "vkCreatePipelineLayout", "Failed to create pipeline layout");

  const auto maybe_empty_pipeline_cache_data =
      PipelineHelpers::try_load_pipeline_cache(name);
  VkPipelineCacheCreateInfo pipeline_cache_create_info{};
  pipeline_cache_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipeline_cache_create_info.initialDataSize =
      maybe_empty_pipeline_cache_data.size();
  pipeline_cache_create_info.pInitialData =
      maybe_empty_pipeline_cache_data.data();

  vkCreatePipelineCache(device.get_device(), &pipeline_cache_create_info,
                        nullptr, &pipeline_cache);

  VkComputePipelineCreateInfo compute_pipeline_create_info{};
  compute_pipeline_create_info.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  compute_pipeline_create_info.layout = pipeline_layout;

  VkPipelineShaderStageCreateInfo shader_stage_create_info{};
  shader_stage_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shader_stage_create_info.module =
      shader.get_shader_module(Shader::Type::Compute).value();
  shader_stage_create_info.pName = "main";

  compute_pipeline_create_info.stage = shader_stage_create_info;

  verify(vkCreateComputePipelines(device.get_device(), pipeline_cache, 1,
                                  &compute_pipeline_create_info, nullptr,
                                  &pipeline),
         "vkCreateComputePipelines", "Failed to create compute pipeline");
}

GraphicsPipeline::GraphicsPipeline(
    const Device &dev, const GraphicsPipelineConfiguration &configuration)
    : device(&dev), name(configuration.name) {
  construct_pipeline(configuration);
}

GraphicsPipeline::~GraphicsPipeline() {
  auto vk_device = device->get_device();
  vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
  vkDestroyPipelineCache(vk_device, pipeline_cache, nullptr);
  vkDestroyPipeline(vk_device, pipeline, nullptr);
};

auto GraphicsPipeline::bind(const CommandBuffer &buffer) const -> void {
  vkCmdBindPipeline(buffer.get_command_buffer(),
                    VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

auto GraphicsPipeline::construct(
    const Device &dev, const GraphicsPipelineConfiguration &configuration)
    -> Scope<GraphicsPipeline> {
  return Scope<GraphicsPipeline>{
      new GraphicsPipeline(dev, configuration),
  };
}

auto GraphicsPipeline::initialise_blend_states(
    const GraphicsPipelineConfiguration &configuration)
    -> std::vector<VkPipelineColorBlendAttachmentState> {
  const auto &fb_props = configuration.framebuffer->get_properties();
  const std::size_t color_attachment_count =
      configuration.framebuffer->get_colour_attachment_count();
  std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states(
      color_attachment_count);
  static constexpr auto blend_all_factors =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  for (usize i = 0; i < color_attachment_count; i++) {
    blend_attachment_states[i].colorWriteMask = blend_all_factors;

    if (!fb_props.blend) {
      blend_attachment_states[i].blendEnable = VK_FALSE;
      break;
    }

    const auto &attachment_spec = fb_props.attachments.attachments[i];
    FramebufferBlendMode blend_mode =
        fb_props.blend_mode == FramebufferBlendMode::None
            ? attachment_spec.blend_mode
            : fb_props.blend_mode;

    blend_attachment_states[i].blendEnable =
        static_cast<VkBool32>(attachment_spec.blend);
    blend_attachment_states[i].colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachment_states[i].alphaBlendOp = VK_BLEND_OP_ADD;
    blend_attachment_states[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment_states[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    switch (blend_mode) {
    case FramebufferBlendMode::OneMinusSourceAlpha:
      blend_attachment_states[i].srcColorBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      blend_attachment_states[i].dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend_attachment_states[i].srcAlphaBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      blend_attachment_states[i].dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      break;
    case FramebufferBlendMode::OneZero:
      blend_attachment_states[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blend_attachment_states[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      break;
    case FramebufferBlendMode::ZeroSourceColor:
      blend_attachment_states[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      blend_attachment_states[i].dstColorBlendFactor =
          VK_BLEND_FACTOR_SRC_COLOR;
      break;
    default:
      assert(false);
    }
  }

  return blend_attachment_states;
}

auto GraphicsPipeline::construct_pipeline(
    const GraphicsPipelineConfiguration &configuration) -> void {

  std::vector dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_LINE_WIDTH,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = static_cast<u32>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  const auto &bindings = configuration.layout.construct_binding();
  VkVertexInputBindingDescription binding_description{};
  binding_description.binding = bindings.binding;
  binding_description.stride = bindings.stride;
  binding_description.inputRate = bindings.input_rate == InputRate::Vertex
                                      ? VK_VERTEX_INPUT_RATE_VERTEX
                                      : VK_VERTEX_INPUT_RATE_INSTANCE;

  std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};
  u32 location = 0;
  for (const auto &attribute : configuration.layout.elements) {
    auto &[attribute_location, binding, format, offset] =
        attribute_descriptions.emplace_back();
    binding = 0;
    format = PipelineHelpers::to_vulkan_format(attribute.type);
    offset = attribute.offset;
    attribute_location = location++;
  }

  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions = &binding_description;
  if (configuration.layout.elements.empty()) {
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexBindingDescriptionCount = 0;
  } else {
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<u32>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions =
        attribute_descriptions.data();
  }
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology =
      PipelineHelpers::vk_polygon_topology(configuration.polygon_mode);
  input_assembly.primitiveRestartEnable = static_cast<VkBool32>(false);

  // Prefer extent over props (due to resizing API)
  u32 width{extent.width};
  u32 height{extent.height};

  VkViewport viewport{};
  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = static_cast<float>(width);
  viewport.height = static_cast<float>(height);
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {
      .width = width,
      .height = height,
  };

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = static_cast<VkBool32>(false);
  rasterizer.rasterizerDiscardEnable = static_cast<VkBool32>(false);
  rasterizer.polygonMode =
      PipelineHelpers::vk_polygon_mode(configuration.polygon_mode);
  rasterizer.lineWidth = configuration.line_width;
  rasterizer.cullMode =
      PipelineHelpers::to_vulkan_cull_mode(configuration.cull_mode);
  rasterizer.frontFace =
      PipelineHelpers::to_vulkan_face_mode(configuration.face_mode);
  rasterizer.depthBiasEnable = static_cast<VkBool32>(false);
  rasterizer.depthBiasConstantFactor = 0.0F;
  rasterizer.depthBiasClamp = 0.0F;
  rasterizer.depthBiasSlopeFactor = 0.0F;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
  depth_stencil_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_create_info.depthTestEnable =
      static_cast<VkBool32>(configuration.test_depth);
  depth_stencil_state_create_info.depthWriteEnable =
      static_cast<VkBool32>(configuration.write_depth);
  depth_stencil_state_create_info.depthCompareOp =
      PipelineHelpers::to_vulkan_comparison(
          configuration.depth_comparison_operator);
  depth_stencil_state_create_info.depthBoundsTestEnable =
      static_cast<VkBool32>(false);
  depth_stencil_state_create_info.stencilTestEnable =
      static_cast<VkBool32>(false);

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = static_cast<VkBool32>(false);
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0F;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = static_cast<VkBool32>(false);
  multisampling.alphaToOneEnable = static_cast<VkBool32>(false);

  VkPipelineColorBlendStateCreateInfo color_blending{};
  auto blend_states = initialise_blend_states(configuration);
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.logicOpEnable =
      static_cast<VkBool32>(configuration.framebuffer->get_properties().blend);
  color_blending.pAttachments = blend_states.data();
  color_blending.attachmentCount = static_cast<u32>(blend_states.size());

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  std::vector<VkPushConstantRange> result;

  const auto &layouts = configuration.shader->get_descriptor_set_layouts();
  pipeline_layout_info.setLayoutCount = static_cast<u32>(layouts.size());
  pipeline_layout_info.pSetLayouts = layouts.data();
  const auto &push_constant_layout =
      configuration.shader->get_push_constant_ranges();
  // We need to hack this for now.
  if (!push_constant_layout.empty()) {
    result.clear();
    VkPushConstantRange &range = result.emplace_back();
    range.offset = 0;
    range.size = push_constant_layout.at(0).size;
    range.stageFlags = push_constant_layout.at(0).shader_stage;
  }
  pipeline_layout_info.pushConstantRangeCount = static_cast<u32>(result.size());
  pipeline_layout_info.pPushConstantRanges = result.data();

  verify(vkCreatePipelineLayout(device->get_device(), &pipeline_layout_info,
                                nullptr, &pipeline_layout),
         "vkCreatePipelineLayout", "Failed to construct pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_create_info{};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

  std::vector<VkPipelineShaderStageCreateInfo> stage_data{};
  const auto &shader = configuration.shader;
  if (const auto code_or = shader->get_shader_module(Shader::Type::Compute);
      code_or.has_value()) {
    VkPipelineShaderStageCreateInfo &info = stage_data.emplace_back();
    info.module = code_or.value();
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.pName = "main";
  }
  if (const auto code_or = shader->get_shader_module(Shader::Type::Vertex);
      code_or.has_value()) {
    VkPipelineShaderStageCreateInfo &info = stage_data.emplace_back();
    info.module = code_or.value();
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    info.pName = "main";
  }
  if (const auto code_or = shader->get_shader_module(Shader::Type::Fragment);
      code_or.has_value()) {
    VkPipelineShaderStageCreateInfo &info = stage_data.emplace_back();
    info.module = code_or.value();
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    info.pName = "main";
  }

  pipeline_create_info.pStages = stage_data.data();
  pipeline_create_info.stageCount = static_cast<u32>(stage_data.size());
  pipeline_create_info.pVertexInputState = &vertex_input_info;
  pipeline_create_info.pInputAssemblyState = &input_assembly;
  pipeline_create_info.pViewportState = &viewport_state;
  pipeline_create_info.pRasterizationState = &rasterizer;
  pipeline_create_info.pMultisampleState = &multisampling;
  pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
  pipeline_create_info.pColorBlendState = &color_blending;
  pipeline_create_info.pDynamicState = &dynamic_state;
  pipeline_create_info.layout = pipeline_layout;
  pipeline_create_info.renderPass =
      configuration.framebuffer->get_render_pass();
  pipeline_create_info.subpass = 0;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_create_info.basePipelineIndex = -1;

  verify(vkCreateGraphicsPipelines(device->get_device(), pipeline_cache, 1,
                                   &pipeline_create_info, nullptr, &pipeline),
         "vkCreateGraphicsPipelines", "Failed to construct graphics pipeline.");
}

} // namespace Core
