#pragma once

#include "Shader.hpp"
#include "Types.hpp"
#include <vulkan/vulkan_core.h>

namespace Core {

enum class PipelineStage : u8 { Compute, Graphics };

struct PipelineConfiguration {
  std::string name;
  PipelineStage stage{PipelineStage::Compute};
  const Shader *shader{nullptr};

  PipelineConfiguration(std::string name, PipelineStage stage,
                        const Shader *shader)
      : name(std::move(name)), stage(stage), shader(shader) {}
};

class Pipeline {
public:
  explicit Pipeline(const PipelineConfiguration &configuration);
  ~Pipeline();

  [[nodiscard]] auto get_pipeline() const -> VkPipeline { return pipeline; }

  [[nodiscard]] auto get_pipeline_layout() const -> VkPipelineLayout {
    return pipeline_layout;
  }

  [[nodiscard]] auto get_pipeline_cache() const -> VkPipelineCache {
    return pipeline_cache;
  }

private:
  auto construct_pipeline(const PipelineConfiguration &configuration) -> void;

  [[nodiscard]] auto try_load_pipeline_cache(const std::string &name)
      -> std::vector<u8>;
  auto create_default_pipeline_cache() -> void;

  std::string name{};
  VkPipelineLayout pipeline_layout{};
  VkPipelineCache pipeline_cache{};
  VkPipeline pipeline{};
};

} // namespace Core