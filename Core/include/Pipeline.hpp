#pragma once

#include "CommandBuffer.hpp"
#include "Shader.hpp"
#include "Types.hpp"
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

enum class PipelineStage : u8 {
  Graphics,
  Compute,
};

struct PipelineConfiguration {
  std::string name;
  PipelineStage stage{PipelineStage::Compute};
  const Shader &shader;
  const std::span<const VkDescriptorSetLayout> descriptor_set_layouts{};

  PipelineConfiguration(std::string name, PipelineStage stage,
                        const Shader &shader,
                        std::span<const VkDescriptorSetLayout> layouts)
      : name(std::move(name)), stage(stage), shader(shader),
        descriptor_set_layouts(layouts) {}
};

class Pipeline {
public:
  explicit Pipeline(const Device &dev,
                    const PipelineConfiguration &configuration);
  ~Pipeline();

  [[nodiscard]] auto get_pipeline() const -> VkPipeline { return pipeline; }

  [[nodiscard]] auto get_pipeline_layout() const -> VkPipelineLayout {
    return pipeline_layout;
  }

  [[nodiscard]] auto get_pipeline_cache() const -> VkPipelineCache {
    return pipeline_cache;
  }

  auto bind(CommandBuffer &) -> void;

private:
  auto construct_pipeline(const PipelineConfiguration &configuration) -> void;

  [[nodiscard]] auto try_load_pipeline_cache(const std::string &name)
      -> std::vector<u8>;

  const Device &device;
  std::string name{};
  VkPipelineLayout pipeline_layout{};
  VkPipelineCache pipeline_cache{};
  VkPipeline pipeline{};
};

} // namespace Core
