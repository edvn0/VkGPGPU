#pragma once

#include "CommandBuffer.hpp"
#include "ResizeDependent.hpp"
#include "Shader.hpp"
#include "Types.hpp"

#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

namespace PipelineHelpers {
[[nodiscard]] auto try_load_pipeline_cache(const std::string &name)
    -> std::vector<u8>;
}

enum class PipelineBindPoint : std::uint8_t {
  BindPointGraphics = 0,
  BindPointCompute = 1,
};
enum class PolygonMode : std::uint8_t { Fill, Line, Point };
enum class DepthCompareOperator : std::uint8_t {
  None = 0,
  Never,
  NotEqual,
  Less,
  LessOrEqual,
  Greater,
  GreaterOrEqual,
  Equal,
  Always,
};
enum class CullMode : std::uint8_t { Back, Front, None, Both };
enum class FaceMode : std::uint8_t { Clockwise, CounterClockwise };
enum class ElementType : std::uint8_t {
  Float,
  Double,
  Float2,
  Float3,
  Float4,
  Int2,
  Int3,
  Int4,
  Uint,
  Uint2,
  Uint3,
  Uint4,
};
enum class VertexInput : std::uint8_t {
  Position,
  TextureCoordinates,
  Normals,
  Colour,
  Tangent,
  Bitangent,
};

static constexpr auto to_size(ElementType type) -> std::size_t {
  switch (type) {
  case ElementType::Float:
    return sizeof(float) * 1;
  case ElementType::Float2:
    return sizeof(float) * 2;
  case ElementType::Float3:
    return sizeof(float) * 3;
  case ElementType::Float4:
    return sizeof(float) * 4;
  case ElementType::Uint:
    return sizeof(u32);
  default:
    assert(false && "Could not map to size.");
  }
  return std::numeric_limits<u32>::max();
}

struct LayoutElement {
  LayoutElement(ElementType element_type, std::string_view debug = "Empty")
      : type(element_type), debug_name(debug), size(to_size(type)){

                                               };

  ElementType type;
  std::string debug_name;
  u32 size{0};
  u32 offset{0};
};

enum class InputRate : std::uint8_t { Vertex, Instance };

struct VertexBinding {
  u32 binding{0};
  u32 stride{0};
  InputRate input_rate{InputRate::Vertex};
};

struct VertexLayout {
  VertexLayout(std::initializer_list<LayoutElement> elems,
               const VertexBinding &bind = {})
      : elements(elems), binding(bind) {
    for (auto &element : elements) {
      element.offset = total_size;
      total_size += element.size;
    }
    if (binding.stride == 0) {
      binding.stride = total_size;
    }
  }

  template <usize N>
  VertexLayout(std::array<LayoutElement, N> elmns,
               const VertexBinding &bind = {}) {
    elements = {elmns.begin(), elmns.end()};
    for (auto &element : elements) {
      element.offset = total_size;
      total_size += element.size;
    }
    binding.stride = total_size;
  }

  [[nodiscard]] auto empty() const noexcept -> bool { return elements.empty(); }

  [[nodiscard]] auto construct_binding() const -> const VertexBinding & {
    return binding;
  }

  u32 total_size{0};
  std::vector<LayoutElement> elements;
  VertexBinding binding;
};

enum class PipelineStage : u8 {
  Graphics,
  Compute,
};

struct ComputePipelineConfiguration {
  std::string name;
  PipelineStage stage{PipelineStage::Compute};
  const Shader &shader;

  ComputePipelineConfiguration(std::string name, PipelineStage stage,
                               const Shader &shader)
      : name(std::move(name)), stage(stage), shader(shader) {}
};

class ComputePipeline {
public:
  ~ComputePipeline();
  auto on_resize(const Extent<u32> &) -> void {}

  [[nodiscard]] auto get_pipeline() const -> const VkPipeline & {
    return pipeline;
  }
  [[nodiscard]] auto get_pipeline_layout() const -> const VkPipelineLayout & {
    return pipeline_layout;
  }
  [[nodiscard]] auto get_bind_point() const -> const VkPipelineBindPoint & {
    return bind_point;
  }
  [[nodiscard]] auto hash() const noexcept -> usize {
    static constexpr std::hash<std::string> hasher;
    static constexpr std::hash<const void *> void_hasher;
    return hasher(name) ^ void_hasher(static_cast<const void *>(pipeline));
  }

  auto bind(const CommandBuffer &) -> void;

  static auto construct(const Device &dev, const ComputePipelineConfiguration &)
      -> Scope<ComputePipeline>;

private:
  explicit ComputePipeline(const Device &dev,
                           const ComputePipelineConfiguration &);
  auto construct_pipeline(const ComputePipelineConfiguration &) -> void;

  const Device &device;
  std::string name{};
  VkPipelineBindPoint bind_point{VK_PIPELINE_BIND_POINT_COMPUTE};
  VkPipelineLayout pipeline_layout{};
  VkPipelineCache pipeline_cache{};
  VkPipeline pipeline{};
};

class Framebuffer;
struct GraphicsPipelineConfiguration {
  std::string name;
  const Shader *shader{nullptr};

  // The framebuffer is not const because we register a resize dependent
  Framebuffer *framebuffer{nullptr};

  VertexLayout layout{};
  VertexLayout instance_layout{};
  PolygonMode polygon_mode{PolygonMode::Fill};
  float line_width{1.0F};
  DepthCompareOperator depth_comparison_operator{
      DepthCompareOperator::GreaterOrEqual};
  CullMode cull_mode{CullMode::Back};
  FaceMode face_mode{FaceMode::CounterClockwise};
  bool write_depth{true};
  bool test_depth{true};
};
class GraphicsPipeline : public IResizeDependent<Framebuffer> {
public:
  ~GraphicsPipeline() override;
  auto on_resize(const Extent<u32> &) -> void;
  auto resize(const Framebuffer &framebuffer) -> void override;

  [[nodiscard]] auto get_pipeline() const -> const VkPipeline & {
    return pipeline;
  }
  [[nodiscard]] auto get_pipeline_layout() const -> const VkPipelineLayout & {
    return pipeline_layout;
  }
  [[nodiscard]] auto get_bind_point() const -> const VkPipelineBindPoint & {
    return bind_point;
  }
  [[nodiscard]] auto hash() const noexcept -> usize {
    static constexpr std::hash<std::string> hasher;
    static constexpr std::hash<const void *> void_hasher;
    return hasher(configuration.name) ^
           void_hasher(static_cast<const void *>(pipeline));
  }

  auto bind(const CommandBuffer &) const -> void;

  static auto construct(const Device &dev,
                        const GraphicsPipelineConfiguration &)
      -> Scope<GraphicsPipeline>;

private:
  explicit GraphicsPipeline(const Device &dev,
                            const GraphicsPipelineConfiguration &);
  auto construct_pipeline(const GraphicsPipelineConfiguration &) -> void;
  auto initialise_blend_states(const GraphicsPipelineConfiguration &)
      -> std::vector<VkPipelineColorBlendAttachmentState>;
  auto destroy() -> void;

  const Device *device;
  GraphicsPipelineConfiguration configuration;
  VkPipelineBindPoint bind_point{VK_PIPELINE_BIND_POINT_GRAPHICS};
  VkPipelineLayout pipeline_layout{};
  VkPipelineCache pipeline_cache{};
  VkPipeline pipeline{};
};

} // namespace Core
