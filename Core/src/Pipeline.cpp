#include "Pipeline.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Device.hpp"
#include "Filesystem.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <filesystem>
#include <fstream>

namespace Core {

Pipeline::Pipeline(const Device &dev,
                   const PipelineConfiguration &configuration)
    : device(dev), name(configuration.name) {
  construct_pipeline(configuration);
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

Pipeline::~Pipeline() {
  try {
    save_cache(device, pipeline_cache, name);
  } catch (const std::exception &exc) {
    error("Pipeline save_cache exception: {}", exc.what());
  }

  vkDestroyPipelineLayout(device.get_device(), pipeline_layout, nullptr);
  vkDestroyPipelineCache(device.get_device(), pipeline_cache, nullptr);
  vkDestroyPipeline(device.get_device(), pipeline, nullptr);
}

auto Pipeline::try_load_pipeline_cache(const std::string &pipeline_name)
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

auto Pipeline::bind(CommandBuffer &command_buffer) -> void {
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

  verify(vkCreatePipelineLayout(device.get_device(),
                                &pipeline_layout_create_info, nullptr,
                                &pipeline_layout),
         "vkCreatePipelineLayout", "Failed to create pipeline layout");

  auto maybe_empty_pipeline_cache_data = try_load_pipeline_cache(name);
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
  shader_stage_create_info.module = configuration.shader.get_shader_module();
  shader_stage_create_info.pName = "main";

  compute_pipeline_create_info.stage = shader_stage_create_info;

  verify(vkCreateComputePipelines(device.get_device(), pipeline_cache, 1,
                                  &compute_pipeline_create_info, nullptr,
                                  &pipeline),
         "vkCreateComputePipelines", "Failed to create compute pipeline");
}

} // namespace Core
