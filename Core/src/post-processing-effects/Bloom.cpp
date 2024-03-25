#include "SceneRenderer.hpp"

namespace Core {

void SceneRenderer::bloom_pass() {
  static auto first_iteration = true;
  if (first_iteration) {
    first_iteration = false;
    return;
  }

  if (!bloom_settings.enabled)
    return;

  enum class BloomMode : int {
    Prefilter,
    Downsample,
    FirstUpsample,
    Upsample,
  };

  struct {
    glm::vec4 params;
    float LOD = 0.0F;
    BloomMode mode = BloomMode::Prefilter;
  } bloom_compute_push_constants;

  bloom_compute_push_constants.params = {
      bloom_settings.threshold,
      bloom_settings.threshold - bloom_settings.knee,
      bloom_settings.knee * 2.0f,
      0.25f / bloom_settings.knee,
  };
  bloom_compute_push_constants.mode = BloomMode::Prefilter;

  const auto &input_image = geometry_framebuffer->get_image();

  std::array images = {
      &bloom_textures.at(0)->get_image(),
      &bloom_textures.at(1)->get_image(),
      &bloom_textures.at(2)->get_image(),
  };

  const auto &shader = bloom_material->get_shader();

  auto descriptor_image_info = images[0]->get_descriptor_info();
  descriptor_image_info.imageView = images[0]->get_mip_image_view_at(0);

  std::array<VkWriteDescriptorSet, 3> write_descriptors{};

  VkDescriptorSetLayout descriptorSetLayout =
      shader.get_descriptor_set_layouts().at(0);

  VkDescriptorSetAllocateInfo allocation_info = {};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorSetCount = 1;
  allocation_info.pSetLayouts = &descriptorSetLayout;

  compute_command_buffer->begin(current_frame);
  gpu_time_queries.bloom_compute_pass_query =
      compute_command_buffer->begin_timestamp_query();

  // Output image
  VkDescriptorSet descriptor_set =
      device->get_descriptor_resource()->allocate_descriptor_set(
          allocation_info);
  write_descriptors[0] =
      *shader.get_descriptor_set("bloom_output_storage_image", 0);
  write_descriptors[0].dstSet = descriptor_set;
  write_descriptors[0].pImageInfo = &descriptor_image_info;

  // Input image
  write_descriptors[1] =
      *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
  write_descriptors[1].dstSet = descriptor_set;
  write_descriptors[1].pImageInfo = &input_image->get_descriptor_info();

  write_descriptors[2] = *shader.get_descriptor_set("bloom_output_texture", 0);
  write_descriptors[2].dstSet = descriptor_set;
  write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<u32>(write_descriptors.size()),
                         write_descriptors.data(), 0, nullptr);

  u32 workgroups_x =
      bloom_textures[0]->get_extent().width / bloom_workgroup_size;
  u32 workgroups_y =
      bloom_textures[0]->get_extent().height / bloom_workgroup_size;

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-Prefilter");
  bloom_pipeline->bind(*compute_command_buffer);
  vkCmdPushConstants(compute_command_buffer->get_command_buffer(),
                     bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL,
                     0, sizeof(bloom_compute_push_constants),
                     &bloom_compute_push_constants);
  vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                          bloom_pipeline->get_bind_point(),
                          bloom_pipeline->get_pipeline_layout(), 0, 1,
                          &descriptor_set, 0, nullptr);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                workgroups_y, 1);
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-Prefilter");

  {
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barrier.image = images[0]->get_image();
    image_memory_barrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        images[0]->get_properties().mip_info.mips,
        0,
        1,
    };
    image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_memory_barrier);
  }

  bloom_compute_push_constants.mode = BloomMode::Downsample;

  u32 offset = 2;
  u32 mips =
      bloom_textures[0]->get_image().get_properties().mip_info.mips - offset;
  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-DownSample");
  for (u32 i = 1; i < mips; i++) {
    auto [mip_width, mip_height] = bloom_textures[0]->get_mip_size(i);
    workgroups_x =
        static_cast<u32>(glm::ceil(static_cast<float>(mip_width) /
                                   static_cast<float>(bloom_workgroup_size)));
    workgroups_y =
        static_cast<u32>(glm::ceil(static_cast<float>(mip_height) /
                                   static_cast<float>(bloom_workgroup_size)));

    {
      // Output image
      descriptor_image_info.imageView = images[1]->get_mip_image_view_at(i);

      descriptor_set =
          device->get_descriptor_resource()->allocate_descriptor_set(
              allocation_info);
      write_descriptors[0] =
          *shader.get_descriptor_set("bloom_output_storage_image", 0);
      write_descriptors[0].dstSet = descriptor_set;
      write_descriptors[0].pImageInfo = &descriptor_image_info;

      // Input image
      write_descriptors[1] =
          *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
      write_descriptors[1].dstSet = descriptor_set;
      auto descriptor = bloom_textures[0]->get_image().get_descriptor_info();
      // descriptor.sampler = samplerClamp;
      write_descriptors[1].pImageInfo = &descriptor;

      write_descriptors[2] =
          *shader.get_descriptor_set("bloom_output_texture", 0);
      write_descriptors[2].dstSet = descriptor_set;
      write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

      vkUpdateDescriptorSets(device->get_device(),
                             static_cast<u32>(write_descriptors.size()),
                             write_descriptors.data(), 0, nullptr);

      bloom_compute_push_constants.LOD = static_cast<float>(i) - 1.0f;
      vkCmdPushConstants(
          compute_command_buffer->get_command_buffer(),
          bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
          sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
      vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                              bloom_pipeline->get_bind_point(),
                              bloom_pipeline->get_pipeline_layout(), 0, 1,
                              &descriptor_set, 0, nullptr);
      vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                    workgroups_y, 1);
    }

    {
      VkImageMemoryBarrier image_memory_barrier = {};
      image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.image = images[1]->get_image();
      image_memory_barrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          images[1]->get_properties().mip_info.mips,
          0,
          1,
      };
      image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &image_memory_barrier);
    }

    {
      descriptor_image_info.imageView = images[0]->get_mip_image_view_at(i);

      // Output image
      descriptor_set =
          device->get_descriptor_resource()->allocate_descriptor_set(
              allocation_info);
      write_descriptors[0] =
          *shader.get_descriptor_set("bloom_output_storage_image", 0);
      write_descriptors[0].dstSet = descriptor_set;
      write_descriptors[0].pImageInfo = &descriptor_image_info;

      // Input image
      write_descriptors[1] =
          *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
      write_descriptors[1].dstSet = descriptor_set;
      auto descriptor = bloom_textures[1]->get_image().get_descriptor_info();
      // descriptor.sampler = samplerClamp;
      write_descriptors[1].pImageInfo = &descriptor;

      write_descriptors[2] =
          *shader.get_descriptor_set("bloom_output_texture", 0);
      write_descriptors[2].dstSet = descriptor_set;
      write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

      vkUpdateDescriptorSets(device->get_device(),
                             static_cast<u32>(write_descriptors.size()),
                             write_descriptors.data(), 0, nullptr);

      bloom_compute_push_constants.LOD = static_cast<float>(i);
      vkCmdPushConstants(
          compute_command_buffer->get_command_buffer(),
          bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
          sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
      vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                              bloom_pipeline->get_bind_point(),
                              bloom_pipeline->get_pipeline_layout(), 0, 1,
                              &descriptor_set, 0, nullptr);
      vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                    workgroups_y, 1);
    }

    {
      VkImageMemoryBarrier image_memory_barrier = {};
      image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.image = images[0]->get_image();
      image_memory_barrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0,
          images[0]->get_properties().mip_info.mips, 0, 1};
      image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &image_memory_barrier);
    }
  }
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-DownSample");

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-FirstUpsample");
  bloom_compute_push_constants.mode = BloomMode::FirstUpsample;
  workgroups_x *= 2;
  workgroups_y *= 2;

  // Output image
  descriptor_set = device->get_descriptor_resource()->allocate_descriptor_set(
      allocation_info);
  descriptor_image_info.imageView = images[2]->get_mip_image_view_at(mips - 2);

  write_descriptors[0] =
      *shader.get_descriptor_set("bloom_output_storage_image", 0);
  write_descriptors[0].dstSet = descriptor_set;
  write_descriptors[0].pImageInfo = &descriptor_image_info;

  // Input image
  write_descriptors[1] =
      *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
  write_descriptors[1].dstSet = descriptor_set;
  write_descriptors[1].pImageInfo =
      &bloom_textures[0]->get_image().get_descriptor_info();

  write_descriptors[2] = *shader.get_descriptor_set("bloom_output_texture", 0);
  write_descriptors[2].dstSet = descriptor_set;
  write_descriptors[2].pImageInfo = &input_image->get_descriptor_info();

  vkUpdateDescriptorSets(device->get_device(), (u32)write_descriptors.size(),
                         write_descriptors.data(), 0, nullptr);

  bloom_compute_push_constants.LOD--;
  vkCmdPushConstants(compute_command_buffer->get_command_buffer(),
                     bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL,
                     0, sizeof(bloom_compute_push_constants),
                     &bloom_compute_push_constants);

  auto &&[mip_width, mip_height] = bloom_textures[2]->get_mip_size(mips - 2);
  workgroups_x = (u32)glm::ceil((float)mip_width / (float)bloom_workgroup_size);
  workgroups_y =
      (u32)glm::ceil((float)mip_height / (float)bloom_workgroup_size);
  vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                          bloom_pipeline->get_bind_point(),
                          bloom_pipeline->get_pipeline_layout(), 0, 1,
                          &descriptor_set, 0, nullptr);
  vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                workgroups_y, 1);
  {
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barrier.image = images[2]->get_image();
    image_memory_barrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        images[2]->get_properties().mip_info.mips,
        0,
        1,
    };
    image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_memory_barrier);
  }

  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-FirstUpsample");

  SceneRenderer::begin_gpu_debug_frame_marker(*compute_command_buffer,
                                              "Bloom-Upsample");
  bloom_compute_push_constants.mode = BloomMode::Upsample;

  // Upsample
  for (i32 mip = static_cast<i32>(mips) - 3; mip >= 0; mip--) {
    auto [texture_mip_width, texture_mip_height] =
        bloom_textures[2]->get_mip_size(mip);
    workgroups_x =
        (u32)glm::ceil((float)texture_mip_width / (float)bloom_workgroup_size);
    workgroups_y =
        (u32)glm::ceil((float)texture_mip_height / (float)bloom_workgroup_size);

    // Output image
    descriptor_image_info.imageView = images[2]->get_mip_image_view_at(mip);
    auto descriptor_set =
        device->get_descriptor_resource()->allocate_descriptor_set(
            allocation_info);
    write_descriptors[0] =
        *shader.get_descriptor_set("bloom_output_storage_image", 0);
    write_descriptors[0].dstSet = descriptor_set;
    write_descriptors[0].pImageInfo = &descriptor_image_info;

    // Input image
    write_descriptors[1] =
        *shader.get_descriptor_set("bloom_geometry_input_texture", 0);
    write_descriptors[1].dstSet = descriptor_set;
    write_descriptors[1].pImageInfo =
        &bloom_textures[0]->get_image().get_descriptor_info();

    write_descriptors[2] =
        *shader.get_descriptor_set("bloom_output_texture", 0);
    write_descriptors[2].dstSet = descriptor_set;
    write_descriptors[2].pImageInfo = &images[2]->get_descriptor_info();

    vkUpdateDescriptorSets(device->get_device(), (u32)write_descriptors.size(),
                           write_descriptors.data(), 0, nullptr);

    bloom_compute_push_constants.LOD = (float)mip;
    vkCmdPushConstants(
        compute_command_buffer->get_command_buffer(),
        bloom_pipeline->get_pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
        sizeof(bloom_compute_push_constants), &bloom_compute_push_constants);
    vkCmdBindDescriptorSets(compute_command_buffer->get_command_buffer(),
                            bloom_pipeline->get_bind_point(),
                            bloom_pipeline->get_pipeline_layout(), 0, 1,
                            &descriptor_set, 0, nullptr);
    vkCmdDispatch(compute_command_buffer->get_command_buffer(), workgroups_x,
                  workgroups_y, 1);

    {
      VkImageMemoryBarrier image_memory_barrier = {};
      image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.image = images[2]->get_image();
      image_memory_barrier.subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0,
          images[2]->get_properties().mip_info.mips, 0, 1};
      image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(compute_command_buffer->get_command_buffer(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &image_memory_barrier);
    }
  }
  SceneRenderer::end_gpu_debug_frame_marker(*compute_command_buffer,
                                            "Bloom-Upsample");

  compute_command_buffer->end_timestamp_query(
      gpu_time_queries.bloom_compute_pass_query);
  compute_command_buffer->end_and_submit();
}

} // namespace Core
