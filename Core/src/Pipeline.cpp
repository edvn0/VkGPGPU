#include "Pipeline.hpp"

#include "Device.hpp"
#include "Filesystem.hpp"
#include "Logger.hpp"
#include "Verify.hpp"

#include <fstream>

namespace Core {

Pipeline::Pipeline(const PipelineConfiguration &configuration)
    : name(configuration.name) {
  construct_pipeline(configuration);
}

auto save_cache(auto pipeline_cache, const std::string &name) {
  if (name.empty()) {
    return;
  }

  if (std::filesystem::exists(name + ".cache")) {
    info("Pipeline cache exists. Overwriting...");
  }

  const auto output_path = FS::pipeline_cache(name + ".cache");
  debug("Saving pipeline cache to {}", output_path.string());
  std::ofstream file{output_path.c_str(), std::ios::binary};
  if (!file) {
    info("Failed to open pipeline cache file at {}", name + ".cache");
    return;
  }

  auto device = Device::get()->get_device();
  auto pipeline_cache_size = size_t{0};
  verify(vkGetPipelineCacheData(device, pipeline_cache, &pipeline_cache_size,
                                nullptr),
         "vkGetPipelineCacheData", "Failed to get pipeline cache data");

  auto pipeline_cache_data = std::vector<u8>(pipeline_cache_size);
  verify(vkGetPipelineCacheData(device, pipeline_cache, &pipeline_cache_size,
                                pipeline_cache_data.data()),
         "vkGetPipelineCacheData", "Failed to get pipeline cache data");

  file.write(reinterpret_cast<const char *>(pipeline_cache_data.data()),
             pipeline_cache_data.size());
}

Pipeline::~Pipeline() {
  save_cache(pipeline_cache, name);

  vkDestroyPipelineLayout(Device::get()->get_device(), pipeline_layout,
                          nullptr);
  vkDestroyPipelineCache(Device::get()->get_device(), pipeline_cache, nullptr);
  vkDestroyPipeline(Device::get()->get_device(), pipeline, nullptr);
}

auto Pipeline::try_load_pipeline_cache(const std::string &name)
    -> std::vector<u8> {
  auto cache_path = FS::pipeline_cache(name + ".cache");
  if (std::filesystem::exists(cache_path)) {
    info("Pipeline cache exists. Loading...");
    auto cache_file = std::ifstream{cache_path, std::ios::binary};
    if (!cache_file) {
      info("Failed to open pipeline cache file at {}", cache_path.string());
    } else {
      auto cache_data = std::vector<u8>(std::filesystem::file_size(cache_path));
      cache_file.read(reinterpret_cast<char *>(cache_data.data()),
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
  pipeline_layout_create_info.setLayoutCount = 1;
  pipeline_layout_create_info.pSetLayouts =
      &configuration.descriptor_set_layout;

  verify(vkCreatePipelineLayout(Device::get()->get_device(),
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

  vkCreatePipelineCache(Device::get()->get_device(),
                        &pipeline_cache_create_info, nullptr, &pipeline_cache);

  VkComputePipelineCreateInfo compute_pipeline_create_info{};
  compute_pipeline_create_info.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  compute_pipeline_create_info.layout = pipeline_layout;

  VkPipelineShaderStageCreateInfo shader_stage_create_info{};
  shader_stage_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shader_stage_create_info.module = configuration.shader->get_shader_module();
  shader_stage_create_info.pName = "main";

  compute_pipeline_create_info.stage = shader_stage_create_info;

  verify(vkCreateComputePipelines(Device::get()->get_device(), pipeline_cache,
                                  1, &compute_pipeline_create_info, nullptr,
                                  &pipeline),
         "vkCreateComputePipelines", "Failed to create compute pipeline");
}

} // namespace Core